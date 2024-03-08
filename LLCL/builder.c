/*
 This file has the buildLLVM function and a bunch of functions that the BuildLLVM function calls.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <libgen.h>
#include <dirent.h>

#include "compiler.h"
#include "globals.h"
#include "builder.h"
#include "report.h"
#include "utilities.h"
#include "printer.h"

void addDILocation(CharAccumulator *source, int ID, SourceLocation location) {
	CharAccumulator_appendChars(source, ", !dbg !DILocation(line: ");
	CharAccumulator_appendInt(source, location.line);
	CharAccumulator_appendChars(source, ", column: ");
	CharAccumulator_appendInt(source, location.columnStart + 1);
	CharAccumulator_appendChars(source, ", scope: !");
	CharAccumulator_appendInt(source, ID);
	CharAccumulator_appendChars(source, ")");
}

linkedList_Node *scopeObjectToList(ScopeObject *scopeObject) {
	linkedList_Node *list = NULL;
//	ScopeObject *newScopeObject = linkedList_addNode(&list, sizeof(ScopeObject));
//	*newScopeObject = ScopeObject_new(0, NULL, scopeObject, ScopeObjectKind_none);
	return list;
}

/// if the node is a memberAccess operator the function calls itself until it gets to the end of the memberAccess operators
ScopeObject *getScopeObjectFromIdentifierNode(FileInformation *FI, ASTnode *node) {
	if (node->nodeType == ASTnodeType_identifier) {
		ASTnode_identifier *data = (ASTnode_identifier *)node->value;
		
		return scopeObject_getAsAlias(getScopeObjectAliasFromSubString(FI, data->name))->value;
	} else if (node->nodeType == ASTnodeType_infixOperator) {
		ASTnode_infixOperator *data = (ASTnode_infixOperator *)node->value;
		
		if (data->operatorType != ASTnode_infixOperatorType_memberAccess) abort();
		
		return getScopeObjectFromIdentifierNode(FI, (ASTnode *)data->left->data);
	} else {
		abort();
	}
	
	return NULL;
}

void expectUnusedName(FileInformation *FI, SubString *name, SourceLocation location) {
	ScopeObject *scopeObject = getScopeObjectAliasFromSubString(FI, name);
	if (scopeObject != NULL) {
		addStringToReportMsg("the name '");
		addSubStringToReportMsg(name);
		addStringToReportMsg("' is defined multiple times");
		
		addStringToReportIndicator("'");
		addSubStringToReportIndicator(name);
		addStringToReportIndicator("' redefined here");
		compileError(FI, location);
	}
}

/// node is an ASTnode_constrainedType
ScopeObject *getScopeObjectFromConstrainedType(FileInformation *FI, ASTnode *node) {
	if (node->nodeType != ASTnodeType_constrainedType) abort();
	ASTnode_constrainedType *data = (ASTnode_constrainedType *)node->value;
	
	linkedList_Node *returnTypeList = NULL;
	buildLLVM(FI, NULL, NULL, NULL, NULL, &returnTypeList, data->type, 0, 0, 0);
	if (returnTypeList == NULL) abort();
	
	((ScopeObject *)returnTypeList->data)->constraintNodes = data->constraints;
	
	return (ScopeObject *)returnTypeList->data;
}

void applyFacts(FileInformation *FI, ASTnode_infixOperator *operator) {
	if (operator->operatorType != ASTnode_infixOperatorType_equivalent) return;
	if (((ASTnode *)operator->left->data)->nodeType != ASTnodeType_identifier) return;
	if (((ASTnode *)operator->right->data)->nodeType != ASTnodeType_number) return;
	
	ScopeObject *scopeObject = getScopeObjectFromIdentifierNode(FI, (ASTnode *)operator->left->data);
	
	Fact *fact = linkedList_addNode(&scopeObject->factStack[FI->level], sizeof(Fact) + sizeof(Fact_expression));
	
	fact->type = FactType_expression;
	((Fact_expression *)fact->value)->operatorType = operator->operatorType;
	((Fact_expression *)fact->value)->left = NULL;
	((Fact_expression *)fact->value)->rightConstant = (ASTnode *)operator->right->data;
}

int addOperatorResultToType(FileInformation *FI, ScopeObject *type, ASTnode_infixOperatorType operatorType, ASTnode *leftNode, ASTnode *rightNode) {
	if (FI->level <= 0) abort();
	if (leftNode->nodeType != rightNode->nodeType) return 0;
	ASTnodeType sharedNodeType = leftNode->nodeType;
	
	switch (sharedNodeType) {
		case ASTnodeType_number: {
			ASTnode_number *left = (ASTnode_number *)leftNode->value;
			ASTnode_number *right = (ASTnode_number *)rightNode->value;
			
			if (
				operatorType == ASTnode_infixOperatorType_equivalent &&
				left->value == right->value
			) {
				ASTnode *node = safeMalloc(sizeof(ASTnode) + sizeof(ASTnode_bool));
				node->nodeType = ASTnodeType_bool;
				node->location = SourceLocation_new(NULL, 0, 0, 0);
				((ASTnode_bool *)node->value)->isTrue = 1;
				
				Fact_newExpression(&type->factStack[FI->level - 1], ASTnode_infixOperatorType_equivalent, NULL, node);
				return 1;
			}
			return 0;
		}
		
		default: {
			return 0;
		}
	}
}

int addTypeResultAfterOperationToList(FileInformation *FI, linkedList_Node **list, char *name, ASTnode_infixOperatorType operatorType, ScopeObject *left, ScopeObject *right) {
	int resultIsTrue = 0;
	
	ScopeObject *type = linkedList_addNode(list, sizeof(ScopeObject));
	*type = ScopeObject_new(0, NULL, getScopeObjectAliasFromString(FI, name), ScopeObjectKind_none);
	
	int leftIndex = FI->level;
	while (leftIndex > 0) {
		linkedList_Node *leftCurrentFact = left->factStack[leftIndex];
		
		while (leftCurrentFact != NULL) {
			Fact *leftFact = (Fact *)leftCurrentFact->data;
			
			if (leftFact->type == FactType_expression) {
				Fact_expression *leftExpressionFact = (Fact_expression *)leftFact->value;
				
				int rightIndex = FI->level;
				while (rightIndex > 0) {
					linkedList_Node *rightCurrentFact = right->factStack[rightIndex];
					
					while (rightCurrentFact != NULL) {
						Fact *rightFact = (Fact *)rightCurrentFact->data;
						
						if (rightFact->type == FactType_expression) {
							Fact_expression *rightExpressionFact = (Fact_expression *)rightFact->value;
							
							if (
								leftExpressionFact->operatorType == ASTnode_infixOperatorType_equivalent &&
								rightExpressionFact->operatorType == ASTnode_infixOperatorType_equivalent
							) {
								int isTrue = addOperatorResultToType(FI, type, operatorType, leftExpressionFact->rightConstant, rightExpressionFact->rightConstant);
								if (isTrue) resultIsTrue = 1;
							}
						}
						
						rightCurrentFact = rightCurrentFact->next;
					}
					
					rightIndex--;
				}
			}
			
			leftCurrentFact = leftCurrentFact->next;
		}
		
		leftIndex--;
	}
	
	return resultIsTrue;
}

void expectType(FileInformation *FI, ScopeObject *expectedType, ScopeObject *actualType, SourceLocation location) {
	if (expectedType->type != actualType->type) {
		addStringToReportMsg("unexpected type");
		
		addStringToReportIndicator("expected type '");
		addSubStringToReportIndicator(scopeObject_getAsAlias(expectedType->type)->key);
		addStringToReportIndicator("' but got type '");
		addSubStringToReportIndicator(scopeObject_getAsAlias(actualType->type)->key);
		addStringToReportIndicator("'");
		compileError(FI, location);
	}
	
//	if (expectedType->constraintNodes != NULL) {
//		ASTnode *constraintExpectedNode = (ASTnode *)expectedType->constraintNodes->data;
//		if (constraintExpectedNode->nodeType != ASTnodeType_infixOperator) abort();
//		ASTnode_infixOperator *expectedData = (ASTnode_infixOperator *)constraintExpectedNode->value;
//		
//		ASTnode *leftNode = (ASTnode *)expectedData->left->data;
//		ASTnode *rightNode = (ASTnode *)expectedData->right->data;
//		
//		BuilderType *left;
//		if (leftNode->nodeType == ASTnodeType_selfReference) {
//			left = actualType;
//		} else {
//			linkedList_Node *leftTypes = NULL;
//			buildLLVM(FI, NULL, NULL, NULL, NULL, &leftTypes, expectedData->left, 0, 0, 0);
//			left = (BuilderType *)leftTypes->data;
//		}
//		
//		BuilderType *right;
//		if (rightNode->nodeType == ASTnodeType_selfReference) {
//			right = actualType;
//		} else {
//			linkedList_Node *rightTypes = NULL;
//			buildLLVM(FI, NULL, NULL, NULL, NULL, &rightTypes, expectedData->right, 0, 0, 0);
//			right = (BuilderType *)rightTypes->data;
//		}
//		
//		linkedList_Node *resultTypes = NULL;
//		if (!addTypeResultAfterOperationToList(FI, &resultTypes, "Bool", expectedData->operatorType, left, right)) {
//			addStringToReportMsg("constraint not met");
//			compileError(FI, location);
//		}
//	}
}

ScopeObject *addFunctionToList(char *LLVMname, FileInformation *FI, linkedList_Node **list, ASTnode *node) {
	ASTnode_function *data = (ASTnode_function *)node->value;
	
	ScopeObject* returnType = getScopeObjectFromConstrainedType(FI, data->returnType);
	
	int compileTime = 0;
	if (ScopeObjectAlias_hasCoreName(returnType, "Type")) {
		compileTime = 1;
	}
	
	linkedList_Node *argumentTypes = NULL;
	
	// make sure that the types of all arguments actually exist
	linkedList_Node *currentArgument = data->argumentTypes;
	while (currentArgument != NULL) {
		ScopeObject *currentArgumentType = getScopeObjectFromConstrainedType(FI, (ASTnode *)currentArgument->data);
		
		ScopeObject *data = linkedList_addNode(&argumentTypes, sizeof(ScopeObject));
		*data = *currentArgumentType;
		
		currentArgument = currentArgument->next;
	}
	
	char *LLVMreturnType = ScopeObjectAlias_getLLVMname(returnType, FI);
	
	ScopeObject *functionScopeObject = linkedList_addNode(list, sizeof(ScopeObject) + sizeof(ScopeObject_function));
	*functionScopeObject = ScopeObject_new(compileTime, NULL, getTypeType(), ScopeObjectKind_function);
	
	if (compileTime) {
		((ScopeObject_function *)functionScopeObject->value)->LLVMname = "LLVMname";
		((ScopeObject_function *)functionScopeObject->value)->LLVMreturnType = "LLVMreturnType";
	} else {
		((ScopeObject_function *)functionScopeObject->value)->LLVMname = LLVMname;
		((ScopeObject_function *)functionScopeObject->value)->LLVMreturnType = LLVMreturnType;
	}
	((ScopeObject_function *)functionScopeObject->value)->argumentNames = data->argumentNames;
	((ScopeObject_function *)functionScopeObject->value)->argumentTypes = argumentTypes;
	((ScopeObject_function *)functionScopeObject->value)->returnType = returnType;
	((ScopeObject_function *)functionScopeObject->value)->registerCount = 0;
	((ScopeObject_function *)functionScopeObject->value)->debugInformationScopeID = 0;
	
	if (!compileTime && compilerOptions.includeDebugInformation) {
		CharAccumulator_appendChars(FI->LLVMmetadataSource, "\n!");
		CharAccumulator_appendInt(FI->LLVMmetadataSource, FI->metadataCount);
		CharAccumulator_appendChars(FI->LLVMmetadataSource, " = distinct !DISubprogram(");
		CharAccumulator_appendChars(FI->LLVMmetadataSource, "name: \"");
		CharAccumulator_appendChars(FI->LLVMmetadataSource, LLVMname);
		CharAccumulator_appendChars(FI->LLVMmetadataSource, "\", scope: !");
		CharAccumulator_appendInt(FI->LLVMmetadataSource, FI->debugInformationFileScopeID);
		CharAccumulator_appendChars(FI->LLVMmetadataSource, ", file: !");
		CharAccumulator_appendInt(FI->LLVMmetadataSource, FI->debugInformationFileScopeID);
		// so apparently Homebrew crashes without a helpful error message if "type" is not here
		CharAccumulator_appendChars(FI->LLVMmetadataSource, ", type: !DISubroutineType(types: !{}), unit: !");
		CharAccumulator_appendInt(FI->LLVMmetadataSource, FI->debugInformationCompileUnitID);
		CharAccumulator_appendChars(FI->LLVMmetadataSource, ")");
		
		((ScopeObject_function *)functionScopeObject->value)->debugInformationScopeID = FI->metadataCount;
		
		FI->metadataCount++;
	}
	
	return functionScopeObject;
}

//void generateStruct(FileInformation *FI, ContextBinding *structBinding, ASTnode *node, int defineNew) {
//	ContextBinding_struct *structToGenerate = (ContextBinding_struct *)structBinding->value;
//	
//	CharAccumulator_appendChars(FI->topLevelStructSource, "\n\n");
//	CharAccumulator_appendChars(FI->topLevelStructSource, structToGenerate->LLVMname);
//	CharAccumulator_appendChars(FI->topLevelStructSource, " = type { ");
//	
//	int addComma = 0;
//	
//	if (defineNew) {
//		int highestBiteAlign = 0;
//		
//		linkedList_Node *currentPropertyNode = ((ASTnode_struct *)node->value)->block;
//		while (currentPropertyNode != NULL) {
//			ASTnode *propertyNode = (ASTnode *)currentPropertyNode->data;
//			if (propertyNode->nodeType != ASTnodeType_variableDefinition) {
//				addStringToReportMsg("only variable definitions are allowed in a struct");
//				compileError(FI, propertyNode->location);
//			}
//			ASTnode_variableDefinition *propertyData = (ASTnode_variableDefinition *)propertyNode->value;
//			
//			// make sure that there is not already a property in this struct with the same name
//			linkedList_Node *currentPropertyBinding = structToGenerate->propertyBindings;
//			while (currentPropertyBinding != NULL) {
//				ContextBinding *propertyBinding = (ContextBinding *)currentPropertyBinding->data;
//				
//				if (SubString_SubString_cmp(propertyBinding->key, propertyData->name) == 0) {
//					addStringToReportMsg("the name '");
//					addSubStringToReportMsg(propertyData->name);
//					addStringToReportMsg("' is defined multiple times inside a struct");
//					
//					addStringToReportIndicator("'");
//					addSubStringToReportIndicator(propertyData->name);
//					addStringToReportIndicator("' redefined here");
//					compileError(FI,  propertyNode->location);
//				}
//				
//				currentPropertyBinding = currentPropertyBinding->next;
//			}
//			
//			// make sure the type actually exists
//			BuilderType* type = getScopeObjectFromConstrainedType(FI, propertyData->type);
//
//			char *LLVMtype = BuilderType_getLLVMname(type, FI);
//			
//			if (BuilderType_getByteAlign(type) > highestBiteAlign) {
//				highestBiteAlign = BuilderType_getByteAlign(type);
//			}
//			
//			structBinding->byteSize += BuilderType_getByteSize(type);
//			
//			if (addComma) {
//				CharAccumulator_appendChars(FI->topLevelStructSource, ", ");
//			}
//			CharAccumulator_appendChars(FI->topLevelStructSource, LLVMtype);
//			
//			ContextBinding *variableBinding = linkedList_addNode(&structToGenerate->propertyBindings, sizeof(ContextBinding) + sizeof(ContextBinding_variable));
//			
//			variableBinding->originFile = FI;
//			variableBinding->key = propertyData->name;
//			variableBinding->type = ContextBindingType_variable;
//			variableBinding->byteSize = type->binding->byteSize;
//			variableBinding->byteAlign = BuilderType_getByteAlign(type);
//			
//			((ContextBinding_variable *)variableBinding->value)->LLVMRegister = 0;
//			((ContextBinding_variable *)variableBinding->value)->LLVMtype = LLVMtype;
//			((ContextBinding_variable *)variableBinding->value)->type = *type;
//			
//			addComma = 1;
//			
//			currentPropertyNode = currentPropertyNode->next;
//		}
//		
//		structBinding->byteAlign = highestBiteAlign;
//	}
//	
//	else {
//		linkedList_Node *currentPropertyBinding = structToGenerate->propertyBindings;
//		while (currentPropertyBinding != NULL) {
//			ContextBinding *propertyBinding = (ContextBinding *)currentPropertyBinding->data;
//			if (propertyBinding->type != ContextBindingType_variable) abort();
//			ContextBinding_variable *variable = (ContextBinding_variable *)propertyBinding->value;
//			
//			if (addComma) {
//				CharAccumulator_appendChars(FI->topLevelStructSource, ", ");
//			}
//			CharAccumulator_appendChars(FI->topLevelStructSource, variable->LLVMtype);
//			
//			addComma = 1;
//			
//			currentPropertyBinding = currentPropertyBinding->next;
//		}
//		
//		FileInformation_addToDeclaredInLLVM(FI, structBinding);
//	}
//	
//	CharAccumulator_appendChars(FI->topLevelStructSource, " }");
//}

void generateType(FileInformation *FI, CharAccumulator *source, ScopeObject *type) {
	abort();
//	if (type->scopeObject->scopeObjectType == ScopeObjectKind_struct) {
//		if (FI != type->scopeObject->originFile && !FileInformation_declaredInLLVM(FI, type->scopeObject)) {
////			generateStruct(FI, type->scopeObject, NULL, 0);
//		}
//	}
//	CharAccumulator_appendChars(source, ScopeObjectAlias_getLLVMname(type, FI));
}

void buildFunctionCodeBlock(FileInformation *FI, ScopeObject *functionScopeObject, CharAccumulator *outerSource, linkedList_Node *codeBlock, SourceLocation location) {
	ScopeObject_function *function = (ScopeObject_function *)functionScopeObject->value;
	
	int functionHasReturned = buildLLVM(FI, function, outerSource, NULL, NULL, NULL, codeBlock, 0, 0, 0);
	
	if (!functionHasReturned) {
		addStringToReportMsg("function did not return");
		
		addStringToReportIndicator("the compiler cannot guarantee that function the returns");
		compileError(FI, location);
	}
}

void generateFunction(FileInformation *FI, CharAccumulator *outerSource, ScopeObject *functionScopeObject, ASTnode *node, int defineNew) {
//	ScopeObject_function *function = (ScopeObject_function *)functionScopeObject->value;
//	
//	if (functionScopeObject->compileTime) {
//		abort();
//	}
//	
//	if (defineNew) {
//		ASTnode_function *data = (ASTnode_function *)node->value;
//		
//		if (data->external) {
//			CharAccumulator_appendChars(outerSource, "\n\ndeclare ");
//		} else {
//			CharAccumulator_appendChars(outerSource, "\n\ndefine ");
//		}
//	} else {
//		CharAccumulator_appendChars(outerSource, "\n\ndeclare ");
//	}
//	
//	CharAccumulator_appendChars(outerSource, function->LLVMreturnType);
//	CharAccumulator_appendChars(outerSource, " @\"");
//	CharAccumulator_appendChars(outerSource, function->LLVMname);
//	CharAccumulator_appendChars(outerSource, "\"(");
//	
//	CharAccumulator functionSource = {100, 0, 0};
//	CharAccumulator_initialize(&functionSource);
//	
//	linkedList_Node *currentArgumentType = function->argumentTypes;
//	linkedList_Node *currentArgumentName = function->argumentNames;
//	if (currentArgumentType != NULL) {
//		int argumentCount =  linkedList_getCount(&function->argumentTypes);
//		while (1) {
//			ScopeObject *argumentTypeScopeObject = ((ScopeObject *)currentArgumentType->data)->;
//			
//			char *currentArgumentLLVMtype = BuilderType_getLLVMname((BuilderType *)currentArgumentType->data, FI);
//			CharAccumulator_appendChars(outerSource, currentArgumentLLVMtype);
//			
//			if (defineNew) {
//				BuilderType type = *(BuilderType *)currentArgumentType->data;
//				
//				CharAccumulator_appendChars(outerSource, " %");
//				CharAccumulator_appendInt(outerSource, function->registerCount);
//				
//				CharAccumulator_appendChars(&functionSource, "\n\t%");
//				CharAccumulator_appendInt(&functionSource, function->registerCount + argumentCount + 1);
//				CharAccumulator_appendChars(&functionSource, " = alloca ");
//				CharAccumulator_appendChars(&functionSource, currentArgumentLLVMtype);
//				CharAccumulator_appendChars(&functionSource, ", align ");
//				CharAccumulator_appendInt(&functionSource, BuilderType_getByteAlign(&type));
//				
//				CharAccumulator_appendChars(&functionSource, "\n\tstore ");
//				CharAccumulator_appendChars(&functionSource, currentArgumentLLVMtype);
//				CharAccumulator_appendChars(&functionSource, " %");
//				CharAccumulator_appendInt(&functionSource, function->registerCount);
//				CharAccumulator_appendChars(&functionSource, ", ptr %");
//				CharAccumulator_appendInt(&functionSource, function->registerCount + argumentCount + 1);
//				CharAccumulator_appendChars(&functionSource, ", align ");
//				CharAccumulator_appendInt(&functionSource, BuilderType_getByteAlign(&type));
//				
//				// TODO: hack with FI->level to include bindings at FI->level + 1 in expectUnusedName (for arguments with the same name)
//				FI->level++;
//				expectUnusedName(FI, (SubString *)currentArgumentName->data, node->location);
//				FI->level--;
//				
//				ScopeObject *argumentScopeObject = linkedList_addNode(&FI->context.scopeObjects[FI->level + 1], sizeof(ScopeObject) + sizeof(ScopeObject_value));
//				*argumentScopeObject = ScopeObject_new((SubString *)currentArgumentName->data, 0, FI, type, ScopeObjectKind_value);
//				*(ScopeObject_value *)argumentScopeObject->value = ScopeObject_value_new(function->registerCount + argumentCount + 1);
//				
//				function->registerCount++;
//			}
//			
//			if (currentArgumentType->next == NULL) {
//				break;
//			}
//			
//			CharAccumulator_appendChars(outerSource, ", ");
//			currentArgumentType = currentArgumentType->next;
//			currentArgumentName = currentArgumentName->next;
//		}
//		
//		if (defineNew) function->registerCount += argumentCount;
//	}
//	
//	CharAccumulator_appendChars(outerSource, ")");
//	
//	// This function attribute indicates that the function never raises an exception.
//	// If the function does raise an exception, its runtime behavior is undefined.
//	CharAccumulator_appendChars(outerSource, " nounwind");
//	
//	if (defineNew) {
//		ASTnode_function *data = (ASTnode_function *)node->value;
//		
//		if (!data->external) {
//			function->registerCount++;
//			
//			if (compilerOptions.includeDebugInformation) {
//				CharAccumulator_appendChars(outerSource, " !dbg !");
//				CharAccumulator_appendInt(outerSource, function->debugInformationScopeID);
//			}
//			CharAccumulator_appendChars(outerSource, " {");
//			CharAccumulator_appendChars(outerSource, functionSource.data);
//			buildFunctionCodeBlock(FI, functionScopeObject, outerSource, data->codeBlock, node->location);
//			
//			CharAccumulator_appendChars(outerSource, "\n}");
//		}
//	} else {
//		FileInformation_addToDeclaredInLLVM(FI, functionScopeObject);
//	}
//	
//	CharAccumulator_free(&functionSource);
}

FileInformation *importFile(FileInformation *currentFI, CharAccumulator *outerSource, char *path) {
	char* fullpath = realpath(path, NULL);
	
	linkedList_Node *currentFile = alreadyCompiledFiles;
	while (currentFile != NULL) {
		FileInformation *fileInformation = *(FileInformation **)currentFile->data;
		if (strcmp(fileInformation->context.path, fullpath) == 0) {
			if (compilerOptions.verbose) {
				printf("file already compiled: %s\n", fullpath);
			}
			return fileInformation;
		}
		currentFile = currentFile->next;
	}
	
	CharAccumulator *topLevelStructSource = safeMalloc(sizeof(CharAccumulator));
	(*topLevelStructSource) = (CharAccumulator){100, 0, 0};
	CharAccumulator_initialize(topLevelStructSource);
	
	CharAccumulator *topLevelConstantSource = safeMalloc(sizeof(CharAccumulator));
	(*topLevelConstantSource) = (CharAccumulator){100, 0, 0};
	CharAccumulator_initialize(topLevelConstantSource);
	
	CharAccumulator *topLevelFunctionSource = safeMalloc(sizeof(CharAccumulator));
	(*topLevelFunctionSource) = (CharAccumulator){100, 0, 0};
	CharAccumulator_initialize(topLevelFunctionSource);
	
	CharAccumulator *LLVMmetadataSource = safeMalloc(sizeof(CharAccumulator));
	(*LLVMmetadataSource) = (CharAccumulator){100, 0, 0};
	CharAccumulator_initialize(LLVMmetadataSource);
	
	FileInformation *newFI = FileInformation_new(fullpath, topLevelStructSource, topLevelConstantSource, topLevelFunctionSource, LLVMmetadataSource);
	compileFile(newFI);
	
	return newFI;
}

int buildLLVM(FileInformation *FI, ScopeObject_function *outerFunction, CharAccumulator *outerSource, CharAccumulator *innerSource, linkedList_Node *expectedTypes, linkedList_Node **types, linkedList_Node *current, int loadVariables, int withTypes, int withCommas) {	
	FI->level++;
	if (FI->level > maxContextLevel) {
		printf("level (%i) > maxContextLevel (%i)\n", FI->level, maxContextLevel);
		abort();
	}
	
	linkedList_Node **currentExpectedType = NULL;
	if (expectedTypes != NULL) {
		currentExpectedType = &expectedTypes;
	}
	
	int hasReturned = 0;
	
	//
	// main loop
	//
	
	linkedList_Node *mainLoopCurrent = current;
	while (mainLoopCurrent != NULL) {
		ASTnode *node = ((ASTnode *)mainLoopCurrent->data);
		
		if (compilerMode == CompilerMode_query && queryMode == QueryMode_suggestions) {
			if (node->location.line >= queryLine) {
				printKeywords(FI);
				printScopeObjects(FI);
				printf("]");
				exit(0);
			}
		}
		
		switch (node->nodeType) {
			case ASTnodeType_constrainedType: {
				addScopeObjectNone(FI, types, getScopeObjectFromConstrainedType(FI, node));
				break;
			}
			
			case ASTnodeType_struct: {
//				ASTnode_struct *data = (ASTnode_struct *)node->value;
//				
//				ContextBinding *structBinding = linkedList_addNode(&FI->context.scopeObjects[FI->level], sizeof(ContextBinding) + sizeof(ContextBinding_struct));
//				
//				structBinding->originFile = FI;
//				structBinding->key = data->name;
//				structBinding->type = ContextBindingType_struct;
//				structBinding->byteSize = 0;
//				// 1 by default but in generateStruct it is set to the highest byteAlign of any property in the struct
//				structBinding->byteAlign = 1;
//				
//				int strlength = strlen("%struct.");
//				
//				char *LLVMname = safeMalloc(strlength + data->name->length + 1);
//				memcpy(LLVMname, "%struct.", strlength);
//				memcpy(LLVMname + strlength, data->name->start, data->name->length);
//				LLVMname[strlength + data->name->length] = 0;
//				
//				((ContextBinding_struct *)structBinding->value)->LLVMname = LLVMname;
//				((ContextBinding_struct *)structBinding->value)->propertyBindings = NULL;
//				((ContextBinding_struct *)structBinding->value)->methodBindings = NULL;
//				
//				generateStruct(FI, structBinding, node, 1);
				
				abort();
				if (types != NULL) {
					// TODO
				}
				
				break;
			}
			
			case ASTnodeType_function: {
				if (FI->level != 0) {
					addStringToReportMsg("function definitions are only allowed at top level");
					compileError(FI, node->location);
				}
				
				ASTnode_function *data = (ASTnode_function *)node->value;
				
				char *LLVMname = NULL;
				
//				ScopeObject *nextSymbolScopeObject = getContextBindingFromString(FI, "core.nextSymbol");
				
//				if (
//					nextSymbolBinding != NULL &&
//					nextSymbolBinding->type == ContextBindingType_compileTimeSetting &&
//					((ContextBinding_compileTimeSetting *)nextSymbolBinding->value)->value != NULL
//				) {
//					SubString *functionSymbol = ((ContextBinding_compileTimeSetting *)nextSymbolBinding->value)->value;
//					int LLVMnameSize = functionSymbol->length + 1;
//					LLVMname = safeMalloc(LLVMnameSize);
//					snprintf(LLVMname, LLVMnameSize, "%s", functionSymbol->start);
//					
//					((ContextBinding_compileTimeSetting *)nextSymbolBinding->value)->value = NULL;
//				} else {
//					ContextBinding *symbolManglingBinding = getContextBindingFromString(FI, "core.symbolMangling");
//					
//					if (SubString_string_cmp(((ContextBinding_compileTimeSetting *)symbolManglingBinding->value)->value, "true") == 0) {
//						int LLVMnameSize = 1 + data->name->length + 1;
//						LLVMname = safeMalloc(LLVMnameSize);
//						snprintf(LLVMname, LLVMnameSize, "%d%s", FI->ID, data->name->start);
//					} else {
//						int LLVMnameSize = data->name->length + 1;
//						LLVMname = safeMalloc(LLVMnameSize);
//						snprintf(LLVMname, LLVMnameSize, "%s", data->name->start);
//					}
//				}
				
				addFunctionToList(LLVMname, FI, &FI->context.scopeObjects[FI->level], node);
				
				break;
			}
			
			case ASTnodeType_call: {
				ASTnode_call *data = (ASTnode_call *)node->value;
				
				if (data->builtin) {
					
				} else {
					
				}
//
//				CharAccumulator leftSource = {100, 0, 0};
//				CharAccumulator_initialize(&leftSource);
//				
//				linkedList_Node *expectedTypesForCall = NULL;
//				addScopeObjectFromString(FI, &expectedTypesForCall, "Function", NULL);
//				
//				linkedList_Node *leftTypes = NULL;
//				buildLLVM(FI, outerFunction, outerSource, &leftSource, expectedTypesForCall, &leftTypes, data->left, 1, 0, 0);
//				
//				ScopeObject *functionToCallScopeObject = ((ScopeObject *)leftTypes->data)->scopeObject;
//				ScopeObject_function *functionToCall = (ScopeObject_function *)functionToCallScopeObject->value;
//				
//				int expectedArgumentCount = linkedList_getCount(&functionToCall->argumentTypes);
//				int actualArgumentCount = linkedList_getCount(&data->arguments);
//				
//				if (expectedArgumentCount > actualArgumentCount) {
//					addStringToReportMsg("function did not get enough arguments");
//					
//					addStringToReportIndicator("'");
//					addSubStringToReportIndicator(functionToCallScopeObject->key);
//					addStringToReportIndicator("' expected ");
//					addIntToReportIndicator(expectedArgumentCount);
//					addStringToReportIndicator(" argument");
//					if (expectedArgumentCount != 1) addStringToReportIndicator("s");
//					addStringToReportIndicator(" but got ");
//					addIntToReportIndicator(actualArgumentCount);
//					addStringToReportIndicator(" argument");
//					if (actualArgumentCount != 1) addStringToReportIndicator("s");
//					compileError(FI, node->location);
//				}
//				if (expectedArgumentCount < actualArgumentCount) {
//					addStringToReportMsg("function got too many arguments");
//					
//					addStringToReportIndicator("'");
//					addSubStringToReportIndicator(functionToCallScopeObject->key);
//					addStringToReportIndicator("' expected ");
//					addIntToReportIndicator(expectedArgumentCount);
//					addStringToReportIndicator(" argument");
//					if (expectedArgumentCount != 1) addStringToReportIndicator("s");
//					addStringToReportIndicator(" but got ");
//					addIntToReportIndicator(actualArgumentCount);
//					addStringToReportIndicator(" argument");
//					if (actualArgumentCount != 1) addStringToReportIndicator("s");
//					compileError(FI, node->location);
//				}
//				
//				if (expectedTypes != NULL) {
//					expectType(FI, (BuilderType *)expectedTypes->data, &functionToCall->returnType, node->location);
//				}
//				
//				if (types != NULL) {
//					addTypeFromBuilderType(FI, types, &functionToCall->returnType);
//				}
//				
//				// TODO: move this out of ASTnodeType_call
//				if (FI != functionToCallScopeObject->originFile && !FileInformation_declaredInLLVM(FI, functionToCallScopeObject)) {
//					generateFunction(FI, FI->topLevelFunctionSource, functionToCallScopeObject, node, 0);
//				}
//				
//				CharAccumulator newInnerSource = {100, 0, 0};
//				CharAccumulator_initialize(&newInnerSource);
//				
//				buildLLVM(FI, outerFunction, outerSource, &newInnerSource, functionToCall->argumentTypes, NULL, data->arguments, 1, 1, 1);
//				
//				if (outerSource != NULL) {
//					if (strcmp(functionToCall->LLVMreturnType, "void") != 0) {
//						CharAccumulator_appendChars(outerSource, "\n\t%");
//						CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
//						CharAccumulator_appendChars(outerSource, " = call ");
//						
//						if (innerSource != NULL) {
//							if (withTypes) {
//								CharAccumulator_appendChars(innerSource, functionToCall->LLVMreturnType);
//								CharAccumulator_appendChars(innerSource, " ");
//							}
//							CharAccumulator_appendChars(innerSource, "%");
//							CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
//						}
//						
//						outerFunction->registerCount++;
//					} else {
//						CharAccumulator_appendChars(outerSource, "\n\tcall ");
//					}
//					CharAccumulator_appendChars(outerSource, functionToCall->LLVMreturnType);
//					CharAccumulator_appendChars(outerSource, " @\"");
//					CharAccumulator_appendChars(outerSource, functionToCall->LLVMname);
//					CharAccumulator_appendChars(outerSource, "\"(");
//					CharAccumulator_appendChars(outerSource, newInnerSource.data);
//					CharAccumulator_appendChars(outerSource, ")");
//					
//					if (compilerOptions.includeDebugInformation) {
//						addDILocation(outerSource, outerFunction->debugInformationScopeID, node->location);
//					}
//				}
//				
//				CharAccumulator_free(&newInnerSource);
//				
//				CharAccumulator_free(&leftSource);
				
				break;
			}
				
			case ASTnodeType_while: {
				if (outerFunction == NULL) {
					addStringToReportMsg("while loops are only allowed in a function");
					compileError(FI, node->location);
				}
				
				ASTnode_while *data = (ASTnode_while *)node->value;
				
				linkedList_Node *expectedTypesForWhile = NULL;
				addScopeObjectFromString(FI, &expectedTypesForWhile, "Bool", NULL);
				
				CharAccumulator expressionSource = {100, 0, 0};
				CharAccumulator_initialize(&expressionSource);
				
				int jump1 = outerFunction->registerCount;
				outerFunction->registerCount++;
				CharAccumulator jump1_outerSource = {100, 0, 0};
				CharAccumulator_initialize(&jump1_outerSource);
				buildLLVM(FI, outerFunction, &jump1_outerSource, &expressionSource, expectedTypesForWhile, NULL, data->expression, 1, 1, 0);
				
				int jump2 = outerFunction->registerCount;
				outerFunction->registerCount++;
				CharAccumulator jump2_outerSource = {100, 0, 0};
				CharAccumulator_initialize(&jump2_outerSource);
				buildLLVM(FI, outerFunction, &jump2_outerSource, NULL, NULL, NULL, data->codeBlock, 0, 0, 0);
				
				int endJump = outerFunction->registerCount;
				outerFunction->registerCount++;
				
				CharAccumulator_appendChars(outerSource, "\n\tbr label %");
				CharAccumulator_appendInt(outerSource, jump1);
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendInt(outerSource, jump1);
				CharAccumulator_appendChars(outerSource, ":");
				CharAccumulator_appendChars(outerSource, jump1_outerSource.data);
				CharAccumulator_appendChars(outerSource, "\n\tbr ");
				CharAccumulator_appendChars(outerSource, expressionSource.data);
				CharAccumulator_appendChars(outerSource, ", label %");
				CharAccumulator_appendInt(outerSource, jump2);
				CharAccumulator_appendChars(outerSource, ", label %");
				CharAccumulator_appendInt(outerSource, endJump);
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendInt(outerSource, jump2);
				CharAccumulator_appendChars(outerSource, ":");
				CharAccumulator_appendChars(outerSource, jump2_outerSource.data);
				CharAccumulator_appendChars(outerSource, "\n\tbr label %");
				CharAccumulator_appendInt(outerSource, jump1);
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendInt(outerSource, endJump);
				CharAccumulator_appendChars(outerSource, ":");
				
				CharAccumulator_free(&expressionSource);
				CharAccumulator_free(&jump1_outerSource);
				CharAccumulator_free(&jump2_outerSource);
				
				break;
			}
			
			case ASTnodeType_if: {
				if (outerFunction == NULL) {
					addStringToReportMsg("if statements are only allowed in a function");
					compileError(FI, node->location);
				}
				
				ASTnode_if *data = (ASTnode_if *)node->value;
				
				linkedList_Node *expectedTypesForIf = NULL;
				addScopeObjectFromString(FI, &expectedTypesForIf, "Bool", NULL);
				
				CharAccumulator expressionSource = {100, 0, 0};
				CharAccumulator_initialize(&expressionSource);
				
				linkedList_Node *expressionTypes = NULL;
				buildLLVM(FI, outerFunction, outerSource, &expressionSource, expectedTypesForIf, &expressionTypes, data->expression, 1, 1, 0);
				
				int typesCount = linkedList_getCount(&expressionTypes);
				
				if (typesCount == 0) {
					addStringToReportMsg("if statement condition expected a bool but got nothing");
					
					if (data->expression == NULL) {
						addStringToReportIndicator("if statement condition is empty");
					} else if (((ASTnode_infixOperator *)((ASTnode *)data->expression->data)->value)->operatorType == ASTnode_infixOperatorType_assignment) {
						addStringToReportIndicator("did you mean to use two equals?");
					}
					
					compileError(FI, node->location);
				} else if (typesCount > 1) {
					addStringToReportMsg("if statement condition got more than one type");
					
					compileError(FI, node->location);
				}
				
				if (((ASTnode *)data->expression->data)->nodeType == ASTnodeType_infixOperator) {
					FI->level++;
					applyFacts(FI, (ASTnode_infixOperator *)((ASTnode *)data->expression->data)->value);
					FI->level--;
				}
				
				int jump1 = outerFunction->registerCount;
				outerFunction->registerCount++;
				CharAccumulator trueCodeBlockSource = {100, 0, 0};
				CharAccumulator_initialize(&trueCodeBlockSource);
				int trueHasReturned = buildLLVM(FI, outerFunction, &trueCodeBlockSource, NULL, NULL, NULL, data->trueCodeBlock, 0, 0, 0);
				CharAccumulator falseCodeBlockSource = {100, 0, 0};
				CharAccumulator_initialize(&falseCodeBlockSource);
				
				int endJump;
				
				if (data->falseCodeBlock == NULL) {
					if (data->trueCodeBlock == NULL) {
						addStringToReportMsg("empty if statement");
						
						compileWarning(FI, node->location, WarningType_unused);
					}
					
					endJump = outerFunction->registerCount;
					outerFunction->registerCount++;
					
					CharAccumulator_appendChars(outerSource, "\n\tbr ");
					CharAccumulator_appendChars(outerSource, expressionSource.data);
					CharAccumulator_appendChars(outerSource, ", label %");
					CharAccumulator_appendInt(outerSource, jump1);
					CharAccumulator_appendChars(outerSource, ", label %");
					CharAccumulator_appendInt(outerSource, endJump);
					
					CharAccumulator_appendChars(outerSource, "\n\n");
					CharAccumulator_appendInt(outerSource, jump1);
					CharAccumulator_appendChars(outerSource, ":");
					CharAccumulator_appendChars(outerSource, trueCodeBlockSource.data);
					CharAccumulator_appendChars(outerSource, "\n\tbr label %");
					CharAccumulator_appendInt(outerSource, endJump);
				} else {
					int jump2 = outerFunction->registerCount;
					outerFunction->registerCount++;
					int falseHasReturned = buildLLVM(FI, outerFunction, &falseCodeBlockSource, NULL, NULL, NULL, data->falseCodeBlock, 0, 0, 0);
					
					endJump = outerFunction->registerCount;
					outerFunction->registerCount++;
					
					CharAccumulator_appendChars(outerSource, "\n\tbr ");
					CharAccumulator_appendChars(outerSource, expressionSource.data);
					CharAccumulator_appendChars(outerSource, ", label %");
					CharAccumulator_appendInt(outerSource, jump1);
					CharAccumulator_appendChars(outerSource, ", label %");
					CharAccumulator_appendInt(outerSource, jump2);
					
					CharAccumulator_appendChars(outerSource, "\n\n");
					CharAccumulator_appendInt(outerSource, jump1);
					CharAccumulator_appendChars(outerSource, ":");
					CharAccumulator_appendChars(outerSource, trueCodeBlockSource.data);
					CharAccumulator_appendChars(outerSource, "\n\tbr label %");
					CharAccumulator_appendInt(outerSource, endJump);
					
					CharAccumulator_appendChars(outerSource, "\n\n");
					CharAccumulator_appendInt(outerSource, jump2);
					CharAccumulator_appendChars(outerSource, ":");
					CharAccumulator_appendChars(outerSource, falseCodeBlockSource.data);
					CharAccumulator_appendChars(outerSource, "\n\tbr label %");
					CharAccumulator_appendInt(outerSource, endJump);
					
					if (trueHasReturned && falseHasReturned) {
//						hasReturned = 1;
					}
				}
				
				CharAccumulator_appendChars(outerSource, "\n\n");
				CharAccumulator_appendInt(outerSource, endJump);
				CharAccumulator_appendChars(outerSource, ":");
				
				CharAccumulator_free(&expressionSource);
				
				CharAccumulator_free(&trueCodeBlockSource);
				CharAccumulator_free(&falseCodeBlockSource);
				
				linkedList_freeList(&expectedTypesForIf);
				linkedList_freeList(&expressionTypes);
				
				break;
			}
			
			case ASTnodeType_return: {
//				if (outerFunction == NULL) {
//					addStringToReportMsg("return statements are only allowed in a function");
//					compileError(FI, node->location);
//				}
//				
//				ASTnode_return *data = (ASTnode_return *)node->value;
//				
//				if (data->expression == NULL) {
//					if (!BuilderType_hasCoreName(&outerFunction->returnType, "Void")) {
//						addStringToReportMsg("returning Void in a function that does not return Void.");
//						compileError(FI, node->location);
//					}
//					if (outerSource != NULL) {
//						CharAccumulator_appendChars(outerSource, "\n\tret void");
//					}
//				} else {
//					CharAccumulator newInnerSource = {100, 0, 0};
//					CharAccumulator_initialize(&newInnerSource);
//					
//					buildLLVM(FI, outerFunction, outerSource, &newInnerSource, typeToList(outerFunction->returnType), NULL, data->expression, 1, 1, 0);
//					
//					if (outerSource != NULL) {
//						CharAccumulator_appendChars(outerSource, "\n\tret ");
//						CharAccumulator_appendChars(outerSource, newInnerSource.data);
//					}
//					
//					CharAccumulator_free(&newInnerSource);
//				}
//				
//				if (outerSource != NULL && compilerOptions.includeDebugInformation) {
//					addDILocation(outerSource, outerFunction->debugInformationScopeID, node->location);
//				}
//				
//				hasReturned = 1;
//				
//				outerFunction->registerCount++;
				
				break;
			}
				
			case ASTnodeType_variableDefinition: {
//				if (outerFunction == NULL) {
//					addStringToReportMsg("variable definitions are only allowed in a function");
//					compileError(FI, node->location);
//				}
//				
//				ASTnode_variableDefinition *data = (ASTnode_variableDefinition *)node->value;
//				
//				expectUnusedName(FI, data->name, node->location);
//				
//				BuilderType* type = getScopeObjectFromConstrainedType(FI, data->type);
//				
//				CharAccumulator expressionSource = {100, 0, 0};
//				CharAccumulator_initialize(&expressionSource);
//				
//				linkedList_Node *types = NULL;
//				buildLLVM(FI, outerFunction, outerSource, &expressionSource, typeToList(*type), &types, data->expression, 1, 1, 0);
//				
//				if (types != NULL) {
//					memcpy(type->factStack, ((BuilderType *)types->data)->factStack, sizeof(type->factStack));
//				}
//				
//				CharAccumulator_appendChars(outerSource, "\n\t%");
//				CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
//				CharAccumulator_appendChars(outerSource, " = alloca ");
//				generateType(FI, outerSource, type);
//				CharAccumulator_appendChars(outerSource, ", align ");
//				CharAccumulator_appendInt(outerSource, BuilderType_getByteAlign(type));
//				
//				if (data->expression != NULL) {
//					CharAccumulator_appendChars(outerSource, "\n\tstore ");
//					CharAccumulator_appendChars(outerSource, expressionSource.data);
//					CharAccumulator_appendChars(outerSource, ", ptr %");
//					CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
//					CharAccumulator_appendChars(outerSource, ", align ");
//					CharAccumulator_appendInt(outerSource, BuilderType_getByteAlign(type));
//				}
//				
//				ScopeObject *variableScopeObject = linkedList_addNode(&FI->context.scopeObjects[FI->level], sizeof(ScopeObject) + sizeof(ScopeObject_value));
//				*variableScopeObject = ScopeObject_new(data->name, 0, FI, *type, ScopeObjectKind_value);
//				*(ScopeObject_value *)variableScopeObject->value = ScopeObject_value_new(outerFunction->registerCount);
//				
//				outerFunction->registerCount++;
//				
//				CharAccumulator_free(&expressionSource);
				
				break;
			}
			
			case ASTnodeType_constantDefinition: {
				ASTnode_constantDefinition *data = (ASTnode_constantDefinition *)node->value;
				
				expectUnusedName(FI, data->name, node->location);
				
				ScopeObject* type = getScopeObjectFromConstrainedType(FI, data->type);
				
				CharAccumulator expressionSource = {100, 0, 0};
				CharAccumulator_initialize(&expressionSource);
//				
//				linkedList_Node *types = NULL;
//				buildLLVM(FI, outerFunction, outerSource, &expressionSource, scopeObjectToList(type), &types, data->expression, 1, 1, 0);
//				
//				if (types != NULL) {
//					memcpy(type->factStack, ((BuilderType *)types->data)->factStack, sizeof(type->factStack));
//				}
//				
//				ScopeObject *variableScopeObject = linkedList_addNode(&FI->context.scopeObjects[FI->level], sizeof(ScopeObject) + sizeof(ScopeObject_value));
//				*variableScopeObject = ScopeObject_new(data->name, 0, FI, *type, ScopeObjectKind_value);
//				*(ScopeObject_value *)variableScopeObject->value = ScopeObject_value_new(outerFunction->registerCount);
//				
//				outerFunction->registerCount++;
				
				CharAccumulator_free(&expressionSource);
				
				break;
			}
			
			case ASTnodeType_infixOperator: {
				ASTnode_infixOperator *data = (ASTnode_infixOperator *)node->value;
				
				CharAccumulator leftInnerSource = {100, 0, 0};
				CharAccumulator_initialize(&leftInnerSource);
				
				CharAccumulator rightInnerSource = {100, 0, 0};
				CharAccumulator_initialize(&rightInnerSource);
				
				if (data->operatorType == ASTnode_infixOperatorType_assignment) {
					if (outerFunction == NULL) {
						addStringToReportMsg("variable assignments are only allowed in a function");
						compileError(FI, node->location);
					}
					
//					ContextBinding *leftVariable = getContextBindingFromIdentifierNode(FI, (ASTnode *)data->left->data);
					
					CharAccumulator leftSource = {100, 0, 0};
					CharAccumulator_initialize(&leftSource);
					linkedList_Node *leftType = NULL;
					buildLLVM(FI, outerFunction, outerSource, &leftSource, NULL, &leftType, data->left, 0, 0, 0);
					
					CharAccumulator rightSource = {100, 0, 0};
					CharAccumulator_initialize(&rightSource);
					buildLLVM(FI, outerFunction, outerSource, &rightSource, leftType, NULL, data->right, 1, 1, 0);
					
					CharAccumulator_appendChars(outerSource, "\n\tstore ");
					CharAccumulator_appendChars(outerSource, rightSource.data);
					CharAccumulator_appendChars(outerSource, ", ptr ");
					CharAccumulator_appendChars(outerSource, leftSource.data);
					CharAccumulator_appendChars(outerSource, ", align ");
					CharAccumulator_appendInt(outerSource, ScopeObjectAlias_getByteAlign((ScopeObject *)leftType->data));
					
					CharAccumulator_free(&rightSource);
					CharAccumulator_free(&leftSource);
					
					break;
				}
				
//				else if (data->operatorType == ASTnode_infixOperatorType_memberAccess) {
//					ASTnode *rightNode = (ASTnode *)data->right->data;
//					
//					if (rightNode->nodeType != ASTnodeType_identifier) {
//						addStringToReportMsg("right side of memberAccess must be an identifier");
//						
//						addStringToReportIndicator("right side of memberAccess is not an identifier");
//						compileError(FI, rightNode->location);
//					};
//					
//					linkedList_Node *leftTypes = NULL;
//					buildLLVM(FI, outerFunction, outerSource, &leftInnerSource, NULL, &leftTypes, data->left, 0, 0, 0);
//					
//					ContextBinding *structBinding = ((BuilderType *)leftTypes->data)->binding;
//					if (structBinding->type != ContextBindingType_struct) {
//						addStringToReportMsg("left side of member access is not a struct");
//						compileError(FI, ((ASTnode *)data->left->data)->location);
//					}
//					ContextBinding_struct *structData = (ContextBinding_struct *)structBinding->value;
//					
//					ASTnode_identifier *rightData = (ASTnode_identifier *)rightNode->value;
//					
//					int index = 0;
//					
//					linkedList_Node *currentPropertyBinding = structData->propertyBindings;
//					while (1) {
//						if (currentPropertyBinding == NULL) {
//							break;
//						}
//						
//						ContextBinding *propertyBinding = (ContextBinding *)currentPropertyBinding->data;
//						
//						if (SubString_SubString_cmp(propertyBinding->key, rightData->name) == 0) {
//							if (propertyBinding->type == ContextBindingType_variable) {
//								ContextBinding_variable *variable = (ContextBinding_variable *)propertyBinding->value;
//								
//								if (types != NULL) {
//									addTypeFromBuilderType(FI, types, &variable->type);
//								}
//								
//								CharAccumulator_appendChars(outerSource, "\n\t%");
//								CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
//								CharAccumulator_appendChars(outerSource, " = getelementptr inbounds ");
//								CharAccumulator_appendChars(outerSource, structData->LLVMname);
//								CharAccumulator_appendChars(outerSource, ", ptr ");
//								CharAccumulator_appendChars(outerSource, leftInnerSource.data);
//								CharAccumulator_appendChars(outerSource, ", i32 0");
//								CharAccumulator_appendChars(outerSource, ", i32 ");
//								CharAccumulator_appendInt(outerSource, index);
//								
//								if (loadVariables) {
//									CharAccumulator_appendChars(outerSource, "\n\t%");
//									CharAccumulator_appendInt(outerSource, outerFunction->registerCount + 1);
//									CharAccumulator_appendChars(outerSource, " = load ");
//									CharAccumulator_appendChars(outerSource, ((ContextBinding_variable *)propertyBinding->value)->LLVMtype);
//									CharAccumulator_appendChars(outerSource, ", ptr %");
//									CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
//									CharAccumulator_appendChars(outerSource, ", align ");
//									CharAccumulator_appendInt(outerSource, BuilderType_getByteAlign(&variable->type));
//									
//									if (withTypes) {
//										CharAccumulator_appendChars(innerSource, ((ContextBinding_variable *)propertyBinding->value)->LLVMtype);
//										CharAccumulator_appendChars(innerSource, " ");
//									}
//									
//									outerFunction->registerCount++;
//								}
//								
//								if (innerSource) {
//									CharAccumulator_appendChars(innerSource, "%");
//									CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
//								} else {
//									addStringToReportMsg("unused member access");
//									compileWarning(FI, rightNode->location, WarningType_unused);
//								}
//								
//								outerFunction->registerCount++;
//							} else {
//								abort();
//							}
//							
//							break;
//						}
//						
//						currentPropertyBinding = currentPropertyBinding->next;
//						if (propertyBinding->type != ContextBindingType_function) {
//							index++;
//						}
//					}
//					
//					linkedList_Node *currentMethodBinding = structData->methodBindings;
//					while (1) {
//						if (currentMethodBinding == NULL) {
//							break;
//						}
//						
//						ContextBinding *methodBinding = (ContextBinding *)currentMethodBinding->data;
//						if (methodBinding->type != ContextBindingType_function) abort();
//						
//						if (SubString_SubString_cmp(methodBinding->key, rightData->name) == 0) {
//							if (types != NULL) {
//								addTypeFromBinding(FI, types, methodBinding);
//							}
//							break;
//						}
//						
//						currentMethodBinding = currentMethodBinding->next;
//					}
//					
//					if (currentPropertyBinding == NULL && currentMethodBinding == NULL) {
//						addStringToReportMsg("left side of memberAccess must exist");
//						compileError(FI, ((ASTnode *)data->left->data)->location);
//					}
//					
//					CharAccumulator_free(&leftInnerSource);
//					CharAccumulator_free(&rightInnerSource);
//					break;
//				}
				
				else if (data->operatorType == ASTnode_infixOperatorType_cast) {
					linkedList_Node *expectedTypeForLeft = NULL;
					buildLLVM(FI, outerFunction, NULL, NULL, NULL, &expectedTypeForLeft, data->left, 0, 0, 0);
					buildLLVM(FI, outerFunction, outerSource, &leftInnerSource, expectedTypeForLeft, NULL, data->left, 1, 1, 0);
					
					ScopeObject *fromType = (ScopeObject *)expectedTypeForLeft->data;
					
					ScopeObject *toType = getScopeObjectFromConstrainedType(FI, (ASTnode *)data->right->data);
					
					CharAccumulator_appendChars(outerSource, "\n\t%");
					CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, " = ");
					
					// make the type bigger
					if (ScopeObjectAlias_getByteSize(fromType) < ScopeObjectAlias_getByteSize(toType)) {
						CharAccumulator_appendChars(outerSource, "sext ");
					}
					
					// make the type smaller
					else if (ScopeObjectAlias_getByteSize(fromType) > ScopeObjectAlias_getByteSize(toType)) {
						CharAccumulator_appendChars(outerSource, "trunc ");
					}
					
					else {
						addStringToReportMsg("cast not currently supported");
						
						compileError(FI, node->location);
					}
					
					CharAccumulator_appendChars(outerSource, leftInnerSource.data);
					CharAccumulator_appendChars(outerSource, " to ");
					CharAccumulator_appendChars(outerSource, ScopeObjectAlias_getLLVMname(toType, FI));
					
					if (withTypes) {
						CharAccumulator_appendChars(innerSource, ScopeObjectAlias_getLLVMname(toType, FI));
						CharAccumulator_appendChars(innerSource, " ");
					}
					CharAccumulator_appendChars(innerSource, "%");
					CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
					
					outerFunction->registerCount++;
					
					break;
				}
				
				linkedList_Node *leftType = NULL;
				linkedList_Node *rightType = NULL;
				
				linkedList_Node *expectedTypeForLeftAndRight = NULL;
				char *expectedLLVMtype = NULL;
				if (expectedTypes != NULL) {
					expectedLLVMtype = ScopeObjectAlias_getLLVMname((ScopeObject *)expectedTypes->data, FI);
				}
				
				if (
					(
					 expectedTypes == NULL && types != NULL
					) ||
					(
						data->operatorType == ASTnode_infixOperatorType_equivalent ||
						data->operatorType == ASTnode_infixOperatorType_notEquivalent ||
						data->operatorType == ASTnode_infixOperatorType_greaterThan ||
						data->operatorType == ASTnode_infixOperatorType_lessThan
					 )
				) {
					//
					// figure out what the type of both sides should be
					//
					
					buildLLVM(FI, outerFunction, NULL, NULL, NULL, &leftType, data->left, 0, 0, 0);
					buildLLVM(FI, outerFunction, NULL, NULL, NULL, &rightType, data->right, 0, 0, 0);
					
					expectedTypeForLeftAndRight = leftType;
					// if we did not find a number on the left side
					if (!ScopeObjectAlias_isNumber((ScopeObject *)expectedTypeForLeftAndRight->data)) {
						if (!ScopeObjectAlias_hasCoreName((ScopeObject *)expectedTypeForLeftAndRight->data, "__Number")) {
							addStringToReportMsg("left side of operator expected a number");
							compileError(FI, node->location);
						}
						
						// replace expectedTypeForLeftAndRight with the right type
						expectedTypeForLeftAndRight = rightType;
						// if we did not find a number on the right side
						if (!ScopeObjectAlias_isNumber((ScopeObject *)expectedTypeForLeftAndRight->data)) {
							if (!ScopeObjectAlias_hasCoreName((ScopeObject *)expectedTypeForLeftAndRight->data, "__Number")) {
								addStringToReportMsg("right side of operator expected a number");
								compileError(FI, node->location);
							}
							
							// default to Int32
							expectedTypeForLeftAndRight = NULL;
							addScopeObjectFromString(FI, &expectedTypeForLeftAndRight, "Int32", NULL);
						}
					}
				}
				
				// all of these operators are very similar and even use the same 'icmp' instruction
				// https://llvm.org/docs/LangRef.html#fcmp-instruction
				if (
					data->operatorType == ASTnode_infixOperatorType_equivalent ||
					data->operatorType == ASTnode_infixOperatorType_notEquivalent ||
					data->operatorType == ASTnode_infixOperatorType_greaterThan ||
					data->operatorType == ASTnode_infixOperatorType_lessThan
				) {
					buildLLVM(FI, outerFunction, outerSource, &leftInnerSource, expectedTypeForLeftAndRight, NULL, data->left, 1, 0, 0);
					buildLLVM(FI, outerFunction, outerSource, &rightInnerSource, expectedTypeForLeftAndRight, NULL, data->right, 1, 0, 0);
					CharAccumulator_appendChars(outerSource, "\n\t%");
					CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, " = icmp ");
					
					if (data->operatorType == ASTnode_infixOperatorType_equivalent) {
						// eq: yields true if the operands are equal, false otherwise. No sign interpretation is necessary or performed.
						CharAccumulator_appendChars(outerSource, "eq ");
					}
					
					else if (data->operatorType == ASTnode_infixOperatorType_notEquivalent) {
						// ne: yields true if the operands are unequal, false otherwise. No sign interpretation is necessary or performed.
						CharAccumulator_appendChars(outerSource, "ne ");
					}
					
					else if (data->operatorType == ASTnode_infixOperatorType_greaterThan) {
						// sgt: interprets the operands as signed values and yields true if op1 is greater than op2.
						CharAccumulator_appendChars(outerSource, "sgt ");
					}
					
					else if (data->operatorType == ASTnode_infixOperatorType_lessThan) {
						// slt: interprets the operands as signed values and yields true if op1 is less than op2.
						CharAccumulator_appendChars(outerSource, "slt ");
					}
					
					else {
						abort();
					}
					
					if (types != NULL) {
						addScopeObjectFromString(FI, types, "Bool", NULL);
					}
					
					CharAccumulator_appendChars(outerSource, ScopeObjectAlias_getLLVMname((ScopeObject *)expectedTypeForLeftAndRight->data, FI));
				}
				
				else if (
					data->operatorType == ASTnode_infixOperatorType_add ||
					data->operatorType == ASTnode_infixOperatorType_subtract ||
					data->operatorType == ASTnode_infixOperatorType_multiply ||
					data->operatorType == ASTnode_infixOperatorType_divide ||
					data->operatorType == ASTnode_infixOperatorType_modulo
				) {
					if (expectedTypes == NULL && types != NULL) {
						addScopeObjectNone(FI, types, (ScopeObject *)expectedTypeForLeftAndRight->data);
					}
					
					if (expectedTypes != NULL && outerSource != NULL) {
						addScopeObjectNone(FI, &expectedTypeForLeftAndRight, (ScopeObject *)expectedTypes->data);
						
						buildLLVM(FI, outerFunction, outerSource, &leftInnerSource, expectedTypeForLeftAndRight, NULL, data->left, 1, 0, 0);
						buildLLVM(FI, outerFunction, outerSource, &rightInnerSource, expectedTypeForLeftAndRight, NULL, data->right, 1, 0, 0);
						CharAccumulator_appendChars(outerSource, "\n\t%");
						CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
						
						if (data->operatorType == ASTnode_infixOperatorType_add) {
							if (ScopeObjectAlias_isInt((ScopeObject *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = add ");
								if (ScopeObjectAlias_isSignedInt((ScopeObject *)expectedTypeForLeftAndRight->data)) {
									CharAccumulator_appendChars(outerSource, "nsw ");
								}
							} else if (ScopeObjectAlias_isFloat((ScopeObject *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = fadd ");
							} else {
								abort();
							}
						} else if (data->operatorType == ASTnode_infixOperatorType_subtract) {
							if (ScopeObjectAlias_isInt((ScopeObject *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = sub ");
								if (ScopeObjectAlias_isSignedInt((ScopeObject *)expectedTypeForLeftAndRight->data)) {
									CharAccumulator_appendChars(outerSource, "nsw ");
								}
							} else if (ScopeObjectAlias_isFloat((ScopeObject *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = fsub ");
							} else {
								abort();
							}
						} else if (data->operatorType == ASTnode_infixOperatorType_multiply) {
							if (ScopeObjectAlias_isInt((ScopeObject *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = mul ");
								if (ScopeObjectAlias_isSignedInt((ScopeObject *)expectedTypeForLeftAndRight->data)) {
									CharAccumulator_appendChars(outerSource, "nsw ");
								}
							} else if (ScopeObjectAlias_isFloat((ScopeObject *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = fmul ");
							} else {
								abort();
							}
						} else if (data->operatorType == ASTnode_infixOperatorType_divide) {
							if (ScopeObjectAlias_isUnsignedInt((ScopeObject *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = udiv ");
							} else if (ScopeObjectAlias_isSignedInt((ScopeObject *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = sdiv ");
							} else if (ScopeObjectAlias_isFloat((ScopeObject *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = fdiv ");
							} else {
								abort();
							}
						} else if (data->operatorType == ASTnode_infixOperatorType_modulo) {
							if (ScopeObjectAlias_isUnsignedInt((ScopeObject *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = urem ");
							} else if (ScopeObjectAlias_isSignedInt((ScopeObject *)expectedTypeForLeftAndRight->data)) {
								CharAccumulator_appendChars(outerSource, " = srem ");
							} else if (ScopeObjectAlias_isFloat((ScopeObject *)expectedTypeForLeftAndRight->data)) {
								addStringToReportMsg("modulo does not support floats");
								
								compileError(FI, node->location);
							} else {
								abort();
							}
						} else {
							abort();
						}
					}
					
					if (expectedLLVMtype != NULL) CharAccumulator_appendChars(outerSource, expectedLLVMtype);
				}
				
				else if (
					data->operatorType == ASTnode_infixOperatorType_and ||
					data->operatorType == ASTnode_infixOperatorType_or
				) {
					addScopeObjectFromString(FI, &expectedTypeForLeftAndRight, "Bool", NULL);
					
					buildLLVM(FI, outerFunction, outerSource, &leftInnerSource, expectedTypeForLeftAndRight, NULL, data->left, 1, 0, 0);
					buildLLVM(FI, outerFunction, outerSource, &rightInnerSource, expectedTypeForLeftAndRight, NULL, data->right, 1, 0, 0);
					
					if (outerSource != NULL) {
						CharAccumulator_appendChars(outerSource, "\n\t%");
						CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
						if (data->operatorType == ASTnode_infixOperatorType_and) {
							CharAccumulator_appendChars(outerSource, " = and i1");
						} else if (data->operatorType == ASTnode_infixOperatorType_or) {
							CharAccumulator_appendChars(outerSource, " = or i1");
						}
					}
					
					if (types != NULL) {
						addScopeObjectFromString(FI, types, "Bool", NULL);
					}
				}
				
				else {
					abort();
				}
				
				if (outerSource != NULL && innerSource != NULL) {
					CharAccumulator_appendChars(outerSource, " ");
					CharAccumulator_appendChars(outerSource, leftInnerSource.data);
					CharAccumulator_appendChars(outerSource, ", ");
					CharAccumulator_appendChars(outerSource, rightInnerSource.data);
					
					if (withTypes && expectedLLVMtype != NULL) {
						CharAccumulator_appendChars(innerSource, expectedLLVMtype);
						CharAccumulator_appendChars(innerSource, " ");
					}
					CharAccumulator_appendChars(innerSource, "%");
					CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
					
					outerFunction->registerCount++;
				}
				
				CharAccumulator_free(&leftInnerSource);
				CharAccumulator_free(&rightInnerSource);
				
				break;
			}
			
			case ASTnodeType_bool: {
				ASTnode_bool *data = (ASTnode_bool *)node->value;
				
				if (!ScopeObjectAlias_hasCoreName((ScopeObject *)expectedTypes->data, "Bool")) {
					addStringToReportMsg("unexpected type");
					
					addStringToReportIndicator("expected type '");
					addSubStringToReportIndicator(ScopeObjectAlias_getName((ScopeObject *)expectedTypes->data));
					addStringToReportIndicator("' but got a bool");
					compileError(FI, node->location);
				}
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, "i1 ");
				}
				
				if (data->isTrue) {
					CharAccumulator_appendChars(innerSource, "true");	
				} else {
					CharAccumulator_appendChars(innerSource, "false");
				}
				
				if (types != NULL) {
					addScopeObjectFromString(FI, types, "Bool", node);
				}
				
				break;
			}
				
			case ASTnodeType_number: {
				ASTnode_number *data = (ASTnode_number *)node->value;
				
				if (types != NULL) {
					addScopeObjectFromString(FI, types, "__Number", node);
				}
				
				if (expectedTypes != NULL) {
					if (!ScopeObjectAlias_isNumber((ScopeObject *)expectedTypes->data)) {
						addStringToReportMsg("unexpected type");
						
						addStringToReportIndicator("expected type '");
						addSubStringToReportIndicator(ScopeObjectAlias_getName((ScopeObject *)expectedTypes->data));
						addStringToReportIndicator("' but got a number.");
						compileError(FI, node->location);
					}
					
					if (withTypes) {
						char *LLVMtype = ScopeObjectAlias_getLLVMname((ScopeObject *)(*currentExpectedType)->data, FI);
						
						CharAccumulator_appendChars(innerSource, LLVMtype);
						CharAccumulator_appendChars(innerSource, " ");
					}
				}
				
				if (innerSource != NULL) CharAccumulator_appendSubString(innerSource, data->string);
				if (expectedTypes != NULL && ScopeObjectAlias_isFloat((ScopeObject *)expectedTypes->data)) {
					CharAccumulator_appendChars(innerSource, ".0");
				}
				
				break;
			}
				
			case ASTnodeType_string: {
				ASTnode_string *data = (ASTnode_string *)node->value;
				
				if (expectedTypes != NULL && !ScopeObjectAlias_hasCoreName((ScopeObject *)expectedTypes->data, "Pointer")) {
					addStringToReportMsg("unexpected type");
					
					addStringToReportIndicator("expected type '");
					addSubStringToReportIndicator(ScopeObjectAlias_getName((ScopeObject *)expectedTypes->data));
					addStringToReportIndicator("' but got a string");
					compileError(FI, node->location);
				}
				
				// + 1 for the NULL byte
				int stringLength = (unsigned int)(data->value->length + 1);
				
				CharAccumulator string = {100, 0, 0};
				CharAccumulator_initialize(&string);
				
				int i = 0;
				int escaped = 0;
				while (i < data->value->length) {
					char character = data->value->start[i];
					
					if (escaped) {
						if (character == '\\') {
							CharAccumulator_appendChars(&string, "\\\\");
							stringLength--;
						} else if (character == 'n') {
							CharAccumulator_appendChars(&string, "\\0A");
							stringLength--;
						} else {
							printf("unexpected character in string after escape '%c'\n", character);
							compileError(FI, SourceLocation_new(FI, node->location.line, node->location.columnStart + i + 1, node->location.columnEnd - (data->value->length - i)));
						}
						escaped = 0;
					} else {
						if (character == '\\') {
							escaped = 1;
						} else {
							CharAccumulator_appendChar(&string, character);
						}
					}
					
					i++;
				}
				
				if (innerSource != NULL) {
					CharAccumulator_appendChars(FI->topLevelConstantSource, "\n@.str.");
					CharAccumulator_appendInt(FI->topLevelConstantSource, FI->stringCount);
					CharAccumulator_appendChars(FI->topLevelConstantSource, " = private unnamed_addr constant [");
					CharAccumulator_appendInt(FI->topLevelConstantSource, stringLength);
					CharAccumulator_appendChars(FI->topLevelConstantSource, " x i8] c\"");
					CharAccumulator_appendChars(FI->topLevelConstantSource, string.data);
					CharAccumulator_appendChars(FI->topLevelConstantSource, "\\00\""); // the \00 is the NULL byte
					
					if (withTypes) {
						CharAccumulator_appendChars(innerSource, "ptr ");
					}
					CharAccumulator_appendChars(innerSource, "@.str.");
					CharAccumulator_appendInt(innerSource, FI->stringCount);
				}
				
				FI->stringCount++;
				
				CharAccumulator_free(&string);
				
				if (types != NULL) {
					addScopeObjectFromString(FI, types, "Pointer", node);
				}
				
				break;
			}
			
			case ASTnodeType_identifier: {
				ASTnode_identifier *data = (ASTnode_identifier *)node->value;
				
				ScopeObject *variableScopeObject = getScopeObjectFromIdentifierNode(FI, node);
				if (variableScopeObject == NULL) {
					addStringToReportMsg("value not in scope");
					
					addStringToReportIndicator("nothing named '");
					addSubStringToReportIndicator(data->name);
					addStringToReportIndicator("'");
					compileError(FI, node->location);
				}
				
				if (variableScopeObject->scopeObjectKind == ScopeObjectKind_struct) {
					if (types != NULL) {
						addScopeObjectNone(FI, types, variableScopeObject);
					}
				} else if (variableScopeObject->scopeObjectKind == ScopeObjectKind_function) {
					if (expectedTypes != NULL) {
						if (!ScopeObjectAlias_hasCoreName((ScopeObject *)expectedTypes->data, "Function")) {
							addStringToReportMsg("unexpected type");
							
							addStringToReportIndicator("expected type '");
							addSubStringToReportIndicator(ScopeObjectAlias_getName((ScopeObject *)expectedTypes->data));
							addStringToReportIndicator("' but got a function");
							compileError(FI, node->location);
						}
					}
					
					if (types != NULL) {
						addScopeObjectNone(FI, types, variableScopeObject);
					}
				} else if (variableScopeObject->scopeObjectKind == ScopeObjectKind_value) {
					if (variableScopeObject->compileTime) {
						abort();
					}
					ScopeObject_value *value = (ScopeObject_value *)variableScopeObject->value;
					
					if (expectedTypes != NULL) {
						expectType(FI, (ScopeObject *)expectedTypes->data, variableScopeObject->type, node->location);
					}
					
					if (types != NULL) {
						addScopeObjectNone(FI, types, variableScopeObject->type);
					}
					
					if (outerSource != NULL) {
						if (withTypes) {
							CharAccumulator_appendChars(innerSource, ScopeObjectAlias_getLLVMname(variableScopeObject->type, FI));
							CharAccumulator_appendChars(innerSource, " ");
						}
						
						if (innerSource != NULL) CharAccumulator_appendChars(innerSource, "%");
						
						if (loadVariables) {
							CharAccumulator_appendChars(outerSource, "\n\t%");
							CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
							CharAccumulator_appendChars(outerSource, " = load ");
							CharAccumulator_appendChars(outerSource, ScopeObjectAlias_getLLVMname(variableScopeObject->type, FI));
							CharAccumulator_appendChars(outerSource, ", ptr %");
							CharAccumulator_appendInt(outerSource, value->LLVMRegister);
							CharAccumulator_appendChars(outerSource, ", align ");
							CharAccumulator_appendInt(outerSource, ScopeObjectAlias_getByteAlign(variableScopeObject->type));
							
							if (innerSource != NULL) CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
							
							outerFunction->registerCount++;
						} else {
							if (innerSource != NULL) CharAccumulator_appendInt(innerSource, value->LLVMRegister);
						}
					}
				} else {
					abort();
				}
				
				break;
			}
			
//			case ASTnodeType_subscript: {
//				ASTnode_subscript *data = (ASTnode_subscript *)node->value;
//				
//				break;
//			}
			
			default: {
				printf("unknown node type: %u\n", node->nodeType);
				compileError(FI, node->location);
				break;
			}
		}
		
		if (compilerMode == CompilerMode_query && queryMode == QueryMode_hover) {
			if (node->location.line == queryLine && node->location.columnStart <= queryColumn && node->location.columnEnd >= queryColumn) {
				// if it is an identifier
				if (node->nodeType == ASTnodeType_identifier) {
					ASTnode_identifier *queryData = (ASTnode_identifier *)node->value;
					printScopeObjectAlias(FI, getScopeObjectAliasFromSubString(FI, queryData->name));
//					printf("]");
//					exit(0);
				}
			}
		}
		
		if (innerSource != NULL && withCommas && mainLoopCurrent->next != NULL) {
			CharAccumulator_appendChars(innerSource, ",");
		}
		
		mainLoopCurrent = mainLoopCurrent->next;
		if (currentExpectedType != NULL) {
			*currentExpectedType = (*currentExpectedType)->next;
		}
	}
	
	//
	// after loop
	//
	
	if (FI->level == 0) {
		linkedList_Node *afterLoopCurrent = current;
		while (afterLoopCurrent != NULL) {
			ASTnode *node = ((ASTnode *)afterLoopCurrent->data);
			
			
			
			afterLoopCurrent = afterLoopCurrent->next;
		}
	}
	
	if (FI->level != 0) {
		linkedList_freeList(&FI->context.scopeObjects[FI->level]);
	}
	
	FI->level--;
	
	return hasReturned;
}

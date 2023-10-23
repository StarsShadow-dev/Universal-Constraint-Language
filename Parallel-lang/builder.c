/*
 This file has the buildLLVM function and a bunch of functions that the BuildLLVM function calls.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "compiler.h"
#include "globals.h"
#include "builder.h"
#include "report.h"
#include "utilities.h"

void addDILocation(CharAccumulator *source, int ID, SourceLocation location) {
	CharAccumulator_appendChars(source, ", !dbg !DILocation(line: ");
	CharAccumulator_appendInt(source, location.line);
	CharAccumulator_appendChars(source, ", column: ");
	CharAccumulator_appendInt(source, location.columnStart + 1);
	CharAccumulator_appendChars(source, ", scope: !");
	CharAccumulator_appendInt(source, ID);
	CharAccumulator_appendChars(source, ")");
}

void addContextBinding_simpleType(linkedList_Node **context, char *name, char *LLVMtype, int byteSize, int byteAlign) {
	SubString *key = safeMalloc(sizeof(SubString));
	key->start = name;
	key->length = (int)strlen(name);
	
	ContextBinding *data = linkedList_addNode(context, sizeof(ContextBinding) + sizeof(ContextBinding_simpleType));
	
	data->key = key;
	data->type = ContextBindingType_simpleType;
	data->byteSize = byteSize;
	data->byteAlign = byteAlign;
	((ContextBinding_simpleType *)data->value)->LLVMtype = LLVMtype;
}

void addContextBinding_macro(ModuleInformation *MI, char *name) {
	SubString *key = safeMalloc(sizeof(SubString));
	key->start = name;
	key->length = (int)strlen(name);
	
	ContextBinding *macroData = linkedList_addNode(&MI->context.bindings[MI->level], sizeof(ContextBinding) + sizeof(ContextBinding_macro));

	macroData->key = key;
	macroData->type = ContextBindingType_macro;
	// I do not think I need to set byteSize or byteAlign to anything specific
	macroData->byteSize = 0;
	macroData->byteAlign = 0;

	((ContextBinding_macro *)macroData->value)->originModule = MI;
	((ContextBinding_macro *)macroData->value)->codeBlock = NULL;
}

ContextBinding *getContextBindingFromString(ModuleInformation *MI, char *key) {
	int index = MI->level;
	while (index >= 0) {
		linkedList_Node *current = MI->context.bindings[index];
		
		while (current != NULL) {
			ContextBinding *binding = ((ContextBinding *)current->data);
			
			if (SubString_string_cmp(binding->key, key) == 0) {
				return binding;
			}
			
			current = current->next;
		}
		
		index--;
	}
	
	// the __core__ module can be implicitly accessed
	linkedList_Node *currentModule = MI->context.importedModules;
	while (currentModule != NULL) {
		ModuleInformation *moduleInformation = *(ModuleInformation **)currentModule->data;
		
		if (strcmp(moduleInformation->name, "__core__") != 0) {
			currentModule = currentModule->next;
			continue;
		}
		
		linkedList_Node *current = moduleInformation->context.bindings[0];
		
		while (current != NULL) {
			ContextBinding *binding = ((ContextBinding *)current->data);
			
			if (SubString_string_cmp(binding->key, key) == 0) {
				return binding;
			}
			
			current = current->next;
		}
		
		break;
	}

	return NULL;
}

ContextBinding *getContextBindingFromSubString(ModuleInformation *MI, SubString *key) {
	int index = MI->level;
	while (index >= 0) {
		linkedList_Node *current = MI->context.bindings[index];
		
		while (current != NULL) {
			ContextBinding *binding = ((ContextBinding *)current->data);
			
			if (SubString_SubString_cmp(binding->key, key) == 0) {
				return binding;
			}
			
			current = current->next;
		}
		
		index--;
	}
	
	// the __core__ module can be implicitly accessed
	linkedList_Node *currentModule = MI->context.importedModules;
	while (currentModule != NULL) {
		ModuleInformation *moduleInformation = *(ModuleInformation **)currentModule->data;
		
		if (strcmp(moduleInformation->name, "__core__") != 0) {
			currentModule = currentModule->next;
			continue;
		}
		
		linkedList_Node *current = moduleInformation->context.bindings[0];
		
		while (current != NULL) {
			ContextBinding *binding = ((ContextBinding *)current->data);
			
			if (SubString_SubString_cmp(binding->key, key) == 0) {
				return binding;
			}
			
			current = current->next;
		}
		
		break;
	}
	
	return NULL;
}

/// if the node is a memberAccess operator the function calls itself until it gets to the end of the memberAccess operators
ContextBinding *getContextBindingFromIdentifierNode(ModuleInformation *MI, ASTnode *node) {
	if (node->nodeType == ASTnodeType_identifier) {
		ASTnode_identifier *data = (ASTnode_identifier *)node->value;
		
		return getContextBindingFromSubString(MI, data->name);
	} else if (node->nodeType == ASTnodeType_operator) {
		ASTnode_operator *data = (ASTnode_operator *)node->value;
		
		if (data->operatorType != ASTnode_operatorType_memberAccess) abort();
		
		return getContextBindingFromIdentifierNode(MI, (ASTnode *)data->left->data);
	} else {
		abort();
	}
	
	return NULL;
}

int ifTypeIsNamed(BuilderType *actualType, char *expectedTypeString) {
	if (SubString_string_cmp(actualType->binding->key, expectedTypeString) == 0) {
		return 1;
	}
	
	return 0;
}

SubString *getTypeAsSubString(BuilderType *type) {
	return type->binding->key;
}

void addTypeFromString(ModuleInformation *MI, linkedList_Node **list, char *string) {
	ContextBinding *binding = getContextBindingFromString(MI, string);
	if (binding == NULL) abort();

	BuilderType *data = linkedList_addNode(list, sizeof(BuilderType));
	data->binding = binding;
	data->constraintNodes = NULL;
}

void addTypeFromBuilderType(ModuleInformation *MI, linkedList_Node **list, BuilderType *type) {
	BuilderType *data = linkedList_addNode(list, sizeof(BuilderType));
	data->binding = type->binding;
	data->constraintNodes = type->constraintNodes;
}

void addTypeFromBinding(ModuleInformation *MI, linkedList_Node **list, ContextBinding *binding) {
	BuilderType *data = linkedList_addNode(list, sizeof(BuilderType));
	data->binding = binding;
	data->constraintNodes = NULL;
}

linkedList_Node *typeToList(BuilderType type) {
	linkedList_Node *list = NULL;
	
	BuilderType *data = linkedList_addNode(&list, sizeof(BuilderType));
	data->binding = type.binding;
	
	return list;
}

void expectUnusedName(ModuleInformation *MI, SubString *name, SourceLocation location) {
	ContextBinding *binding = getContextBindingFromSubString(MI, name);
	if (binding != NULL) {
		addStringToReportMsg("the name '");
		addSubStringToReportMsg(name);
		addStringToReportMsg("' is defined multiple times");
		
		addStringToReportIndicator("'");
		addSubStringToReportIndicator(name);
		addStringToReportIndicator("' redefined here");
		compileError(MI, location);
	}
}

// node is a ASTnode_constrainedType
BuilderType *getType(ModuleInformation *MI, ASTnode *node) {
	if (node->nodeType != ASTnodeType_constrainedType) abort();
	ASTnode_constrainedType *data = (ASTnode_constrainedType *)node->value;
	
	linkedList_Node *returnTypeList = NULL;
	buildLLVM(MI, NULL, NULL, NULL, NULL, &returnTypeList, data->type, 0, 0, 0);
	if (returnTypeList == NULL) abort();
	
	if (
		((BuilderType *)returnTypeList->data)->binding->type != ContextBindingType_simpleType &&
		((BuilderType *)returnTypeList->data)->binding->type != ContextBindingType_struct
	) {
		addStringToReportMsg("expected type");
		
		addStringToReportIndicator("'");
		addSubStringToReportIndicator(((BuilderType *)returnTypeList->data)->binding->key);
		addStringToReportIndicator("' is not a type");
		compileError(MI, node->location);
	}
	
	((BuilderType *)returnTypeList->data)->constraintNodes = data->constraints;
	
	return (BuilderType *)returnTypeList->data;
}

void applyFacts(ModuleInformation *MI, ASTnode_operator *operator) {
	if (operator->operatorType != ASTnode_operatorType_equivalent) return;
	if (((ASTnode *)operator->left->data)->nodeType != ASTnodeType_identifier) return;
	if (((ASTnode *)operator->right->data)->nodeType != ASTnodeType_number) return;
	
	ContextBinding *variableBinding = getContextBindingFromIdentifierNode(MI, (ASTnode *)operator->left->data);
	
	ContextBinding_variable *variable = (ContextBinding_variable *)variableBinding->value;
	
	Fact *fact = linkedList_addNode(&variable->factStack[MI->level], sizeof(Fact) + sizeof(Fact_expression));
	
	fact->type = FactType_expression;
	((Fact_expression *)fact->value)->operatorType = operator->operatorType;
	((Fact_expression *)fact->value)->left = (ASTnode *)operator->left->data;
	((Fact_expression *)fact->value)->rightConstant = (ASTnode *)operator->right->data;
}

int isExpressionTrue(ModuleInformation *MI, ContextBinding_variable *self, ASTnode_operator *operator) {
	if (self == NULL) abort();
	
	ASTnode *leftNode = (ASTnode *)operator->left->data;
	ASTnode *rightNode = (ASTnode *)operator->right->data;
	
	if (leftNode->nodeType != ASTnodeType_selfReference) return 0;
	if (rightNode->nodeType != ASTnodeType_number) return 0;
	
	if (operator->operatorType == ASTnode_operatorType_equivalent) {
		if (leftNode->nodeType == ASTnodeType_selfReference && rightNode->nodeType == ASTnodeType_number) {
			int index = MI->level;
			while (index > 0) {
				linkedList_Node *currentFact = self->factStack[index];
				
				while (currentFact != NULL) {
					Fact *fact = (Fact *)currentFact->data;
					
					if (fact->type == FactType_expression) {
						Fact_expression *expressionFact = (Fact_expression *)fact->value;
						if (expressionFact->rightConstant->nodeType != ASTnodeType_number) abort();
						
						if (
							((ASTnode_number *)rightNode->value)->value ==
							((ASTnode_number *)expressionFact->rightConstant->value)->value
						) {
							return 1;
						}
					}
					
					currentFact = currentFact->next;
				}
				
				index--;
			}
		}
	}
	
	return 0;
}

void expectType(ModuleInformation *MI, ContextBinding_variable *self, BuilderType *expectedType, BuilderType *actualType, SourceLocation location) {
	if (expectedType->binding != actualType->binding) {
		addStringToReportMsg("unexpected type");
		
		addStringToReportIndicator("expected type '");
		addSubStringToReportIndicator(expectedType->binding->key);
		addStringToReportIndicator("' but got type '");
		addSubStringToReportIndicator(actualType->binding->key);
		addStringToReportIndicator("'");
		compileError(MI, location);
	}
	
	if (expectedType->constraintNodes != NULL) {
		ASTnode *constraintExpectedNode = (ASTnode *)expectedType->constraintNodes->data;
		if (constraintExpectedNode->nodeType != ASTnodeType_operator) abort();
		ASTnode_operator *expectedData = (ASTnode_operator *)constraintExpectedNode->value;
		
		if (!isExpressionTrue(MI, self, expectedData)) {
			addStringToReportMsg("constraint not met");
			compileError(MI, location);
		}
	}
}

char *getLLVMtypeFromBinding(ModuleInformation *MI, ContextBinding *binding) {
	if (binding->type == ContextBindingType_simpleType) {
		return ((ContextBinding_simpleType *)binding->value)->LLVMtype;
	} else if (binding->type == ContextBindingType_struct) {
		return ((ContextBinding_struct *)binding->value)->LLVMname;
	}
	
	abort();
}

BuilderType getTypeFromBinding(ContextBinding *binding) {
	return (BuilderType){binding};
}

void getTypeDescription(ModuleInformation *MI, CharAccumulator *charAccumulator, BuilderType *builderType) {}

void getVariableDescription(ModuleInformation *MI, CharAccumulator *charAccumulator, ContextBinding *variableBinding) {
	if (variableBinding->type != ContextBindingType_variable) abort();
	ContextBinding_variable *variable = (ContextBinding_variable *)variableBinding->value;
	
	CharAccumulator_appendChars(charAccumulator, "variable description for '");
	CharAccumulator_appendSubString(charAccumulator, variableBinding->key);
	CharAccumulator_appendChars(charAccumulator, "'\nbyteSize = ");
	CharAccumulator_appendInt(charAccumulator, variableBinding->byteSize);
	CharAccumulator_appendChars(charAccumulator, "\nbyteAlign = ");
	CharAccumulator_appendInt(charAccumulator, variableBinding->byteAlign);
	CharAccumulator_appendChars(charAccumulator, "\n\n");
	
	int index = 0;
	while (index < MI->level) {
		linkedList_Node *currentFact = variable->factStack[index];
		
		if (currentFact == NULL) {
			index++;
			continue;
		}
		
		CharAccumulator_appendChars(charAccumulator, "level ");
		CharAccumulator_appendInt(charAccumulator, index);
		CharAccumulator_appendChars(charAccumulator, ": ");
		
		while (currentFact != NULL) {
			Fact *fact = (Fact *)currentFact->data;
			
			if (fact->type == FactType_expression) {
				Fact_expression *expressionFact = (Fact_expression *)fact->value;
				if (expressionFact->left->nodeType != ASTnodeType_identifier) abort();
				if (expressionFact->rightConstant->nodeType != ASTnodeType_number) abort();
				
				CharAccumulator_appendChars(charAccumulator, "(");
				
				if (expressionFact->operatorType == ASTnode_operatorType_equivalent) {
					CharAccumulator_appendSubString(charAccumulator, ((ASTnode_identifier *)expressionFact->left->value)->name);
					CharAccumulator_appendChars(charAccumulator, " == ");
					CharAccumulator_appendInt(charAccumulator, ((ASTnode_number *)expressionFact->rightConstant->value)->value);
				} else {
					abort();
				}
				
				CharAccumulator_appendChars(charAccumulator, ")");
			} else {
				abort();
			}
			
			currentFact = currentFact->next;
		}
		
		CharAccumulator_appendChars(charAccumulator, "\n");
		
		index++;
	}
	
//	CharAccumulator_appendChars(charAccumulator, "\n");
}

ContextBinding *addFunctionToList(char *LLVMname, ModuleInformation *MI, linkedList_Node **list, ASTnode *node) {
	ASTnode_function *data = (ASTnode_function *)node->value;
	
	// make sure that the return type actually exists
	BuilderType* returnType = getType(MI, data->returnType);
	
	linkedList_Node *argumentTypes = NULL;
	
	// make sure that the types of all arguments actually exist
	linkedList_Node *currentArgument = data->argumentTypes;
	while (currentArgument != NULL) {
		BuilderType *currentArgumentType = getType(MI, (ASTnode *)currentArgument->data);
		
		BuilderType *data = linkedList_addNode(&argumentTypes, sizeof(BuilderType));
		*data = *currentArgumentType;
		
		currentArgument = currentArgument->next;
	}
	
	char *LLVMreturnType = getLLVMtypeFromBinding(MI, returnType->binding);
	
	ContextBinding *functionData = linkedList_addNode(list, sizeof(ContextBinding) + sizeof(ContextBinding_function));
	
	functionData->key = data->name;
	functionData->type = ContextBindingType_function;
	// I do not think I need to set byteSize or byteAlign to anything specific
	functionData->byteSize = 0;
	functionData->byteAlign = 0;
	
	((ContextBinding_function *)functionData->value)->LLVMname = LLVMname;
	((ContextBinding_function *)functionData->value)->LLVMreturnType = LLVMreturnType;
	((ContextBinding_function *)functionData->value)->argumentNames = data->argumentNames;
	((ContextBinding_function *)functionData->value)->argumentTypes = argumentTypes;
	((ContextBinding_function *)functionData->value)->returnType = *returnType;
	((ContextBinding_function *)functionData->value)->registerCount = 0;
	
	if (compilerOptions.includeDebugInformation) {
		CharAccumulator_appendChars(MI->LLVMmetadataSource, "\n!");
		CharAccumulator_appendInt(MI->LLVMmetadataSource, MI->metadataCount);
		CharAccumulator_appendChars(MI->LLVMmetadataSource, " = distinct !DISubprogram(");
		CharAccumulator_appendChars(MI->LLVMmetadataSource, "name: \"");
		CharAccumulator_appendChars(MI->LLVMmetadataSource, LLVMname);
		CharAccumulator_appendChars(MI->LLVMmetadataSource, "\", scope: !");
		CharAccumulator_appendInt(MI->LLVMmetadataSource, MI->debugInformationFileScopeID);
		CharAccumulator_appendChars(MI->LLVMmetadataSource, ", file: !");
		CharAccumulator_appendInt(MI->LLVMmetadataSource, MI->debugInformationFileScopeID);
		// so apparently Homebrew crashes without a helpful error message if "type" is not here
		CharAccumulator_appendChars(MI->LLVMmetadataSource, ", type: !DISubroutineType(types: !{}), unit: !");
		CharAccumulator_appendInt(MI->LLVMmetadataSource, MI->debugInformationCompileUnitID);
		CharAccumulator_appendChars(MI->LLVMmetadataSource, ")");
		
		((ContextBinding_function *)functionData->value)->debugInformationScopeID = MI->metadataCount;
		
		MI->metadataCount++;
	}
	
	return functionData;
}

void generateFunction(ModuleInformation *MI, CharAccumulator *outerSource, ContextBinding_function *function, ASTnode *node, int defineNew) {
	if (defineNew) {
		ASTnode_function *data = (ASTnode_function *)node->value;
		
		if (data->external) {
			CharAccumulator_appendSubString(outerSource, ((ASTnode_string *)(((ASTnode *)(data->codeBlock)->data)->value))->value);
			return;
		}
	}
	
	if (defineNew) {
		CharAccumulator_appendChars(outerSource, "\n\ndefine ");
	} else {
		CharAccumulator_appendChars(outerSource, "\n\ndeclare ");
	}
	CharAccumulator_appendChars(outerSource, function->LLVMreturnType);
	CharAccumulator_appendChars(outerSource, " @");
	CharAccumulator_appendChars(outerSource, function->LLVMname);
	CharAccumulator_appendChars(outerSource, "(");
	
	CharAccumulator functionSource = {100, 0, 0};
	CharAccumulator_initialize(&functionSource);
	
	linkedList_Node *currentArgumentType = function->argumentTypes;
	linkedList_Node *currentArgumentName = function->argumentNames;
	if (currentArgumentType != NULL) {
		int argumentCount =  linkedList_getCount(&function->argumentTypes);
		while (1) {
			ContextBinding *argumentTypeBinding = ((BuilderType *)currentArgumentType->data)->binding;
			
			char *currentArgumentLLVMtype = getLLVMtypeFromBinding(MI, ((BuilderType *)currentArgumentType->data)->binding);
			CharAccumulator_appendChars(outerSource, currentArgumentLLVMtype);
			
			if (defineNew) {
				CharAccumulator_appendChars(outerSource, " %");
				CharAccumulator_appendInt(outerSource, function->registerCount);
				
				CharAccumulator_appendChars(&functionSource, "\n\t%");
				CharAccumulator_appendInt(&functionSource, function->registerCount + argumentCount + 1);
				CharAccumulator_appendChars(&functionSource, " = alloca ");
				CharAccumulator_appendChars(&functionSource, currentArgumentLLVMtype);
				CharAccumulator_appendChars(&functionSource, ", align ");
				CharAccumulator_appendInt(&functionSource, argumentTypeBinding->byteAlign);
				
				CharAccumulator_appendChars(&functionSource, "\n\tstore ");
				CharAccumulator_appendChars(&functionSource, currentArgumentLLVMtype);
				CharAccumulator_appendChars(&functionSource, " %");
				CharAccumulator_appendInt(&functionSource, function->registerCount);
				CharAccumulator_appendChars(&functionSource, ", ptr %");
				CharAccumulator_appendInt(&functionSource, function->registerCount + argumentCount + 1);
				CharAccumulator_appendChars(&functionSource, ", align ");
				CharAccumulator_appendInt(&functionSource, argumentTypeBinding->byteAlign);
				
				expectUnusedName(MI, (SubString *)currentArgumentName->data, node->location);
				
				BuilderType type = *(BuilderType *)currentArgumentType->data;
				
				ContextBinding *argumentVariableData = linkedList_addNode(&MI->context.bindings[MI->level + 1], sizeof(ContextBinding) + sizeof(ContextBinding_variable));
				
				argumentVariableData->key = (SubString *)currentArgumentName->data;
				argumentVariableData->type = ContextBindingType_variable;
				argumentVariableData->byteSize = argumentTypeBinding->byteSize;
				argumentVariableData->byteAlign = argumentTypeBinding->byteAlign;
				
				((ContextBinding_variable *)argumentVariableData->value)->LLVMRegister = function->registerCount + argumentCount + 1;
				((ContextBinding_variable *)argumentVariableData->value)->LLVMtype = currentArgumentLLVMtype;
				((ContextBinding_variable *)argumentVariableData->value)->type = type;
				memset(((ContextBinding_variable *)argumentVariableData->value)->factStack, 0, sizeof(linkedList_Node *[maxContextLevel]));
				
				function->registerCount++;
			}
			
			if (currentArgumentType->next == NULL) {
				break;
			}
			
			CharAccumulator_appendChars(outerSource, ", ");
			currentArgumentType = currentArgumentType->next;
			currentArgumentName = currentArgumentName->next;
		}
		
		if (defineNew) function->registerCount += argumentCount;
	}
	
	CharAccumulator_appendChars(outerSource, ")");
	
	if (defineNew) {
		ASTnode_function *data = (ASTnode_function *)node->value;
		
		function->registerCount++;
		
		if (compilerOptions.includeDebugInformation) {
			CharAccumulator_appendChars(outerSource, " !dbg !");
			CharAccumulator_appendInt(outerSource, function->debugInformationScopeID);
		}
		CharAccumulator_appendChars(outerSource, " {");
		CharAccumulator_appendChars(outerSource, functionSource.data);
		int functionHasReturned = buildLLVM(MI, function, outerSource, NULL, NULL, NULL, data->codeBlock, 0, 0, 0);
		
		if (!functionHasReturned) {
			addStringToReportMsg("function did not return");
			
			addStringToReportIndicator("the compiler cannot guarantee that function '");
			addSubStringToReportIndicator(data->name);
			addStringToReportIndicator("' returns");
			compileError(MI, node->location);
		}
		
		CharAccumulator_appendChars(outerSource, "\n}");
	}
	
	CharAccumulator_free(&functionSource);
}

void generateStruct(ModuleInformation *MI, CharAccumulator *outerSource, ContextBinding *structBinding, ASTnode *node, int defineNew) {
	ContextBinding_struct *structToGenerate = (ContextBinding_struct *)structBinding->value;
	
	CharAccumulator_appendChars(outerSource, "\n\n");
	CharAccumulator_appendChars(outerSource, structToGenerate->LLVMname);
	CharAccumulator_appendChars(outerSource, " = type { ");
	
	int addComma = 0;
	
	if (defineNew) {
		linkedList_Node *currentPropertyNode = ((ASTnode_struct *)node->value)->block;
		while (currentPropertyNode != NULL) {
			ASTnode *propertyNode = (ASTnode *)currentPropertyNode->data;
			if (propertyNode->nodeType != ASTnodeType_variableDefinition) {
				addStringToReportMsg("only variable definitions are allowed in a struct");
				compileError(MI, propertyNode->location);
			}
			ASTnode_variableDefinition *propertyData = (ASTnode_variableDefinition *)propertyNode->value;
			
			// make sure that there is not already a property in this struct with the same name
			linkedList_Node *currentPropertyBinding = structToGenerate->propertyBindings;
			while (currentPropertyBinding != NULL) {
				ContextBinding *propertyBinding = (ContextBinding *)currentPropertyBinding->data;
				
				if (SubString_SubString_cmp(propertyBinding->key, propertyData->name) == 0) {
					addStringToReportMsg("the name '");
					addSubStringToReportMsg(propertyData->name);
					addStringToReportMsg("' is defined multiple times inside a struct");
					
					addStringToReportIndicator("'");
					addSubStringToReportIndicator(propertyData->name);
					addStringToReportIndicator("' redefined here");
					compileError(MI,  propertyNode->location);
				}
				
				currentPropertyBinding = currentPropertyBinding->next;
			}
			
			// make sure the type actually exists
			BuilderType* type = getType(MI, propertyData->type);
			
			char *LLVMtype = getLLVMtypeFromBinding(MI, type->binding);
			
			// if there is a pointer anywhere in the struct then the struct should be aligned by pointer_byteSize
			if (strcmp(LLVMtype, "ptr") == 0) {
				structBinding->byteAlign = pointer_byteSize;
			}
			
			structBinding->byteSize += type->binding->byteSize;
			
			if (addComma) {
				CharAccumulator_appendChars(outerSource, ", ");
			}
			CharAccumulator_appendChars(outerSource, LLVMtype);
			
			ContextBinding *variableBinding = linkedList_addNode(&structToGenerate->propertyBindings, sizeof(ContextBinding) + sizeof(ContextBinding_variable));
			
			variableBinding->key = propertyData->name;
			variableBinding->type = ContextBindingType_variable;
			variableBinding->byteSize = type->binding->byteSize;
			variableBinding->byteAlign = type->binding->byteSize;
			
			((ContextBinding_variable *)variableBinding->value)->LLVMRegister = 0;
			((ContextBinding_variable *)variableBinding->value)->LLVMtype = LLVMtype;
			((ContextBinding_variable *)variableBinding->value)->type = *type;
			memset(((ContextBinding_variable *)variableBinding->value)->factStack, 0, sizeof(linkedList_Node *[maxContextLevel]));
			
			addComma = 1;
			
			currentPropertyNode = currentPropertyNode->next;
		}
	}
	
	else {
		linkedList_Node *currentPropertyBinding = structToGenerate->propertyBindings;
		while (currentPropertyBinding != NULL) {
			ContextBinding *propertyBinding = (ContextBinding *)currentPropertyBinding->data;
			if (propertyBinding->type != ContextBindingType_variable) abort();
			ContextBinding_variable *variable = (ContextBinding_variable *)propertyBinding->value;
			
			if (addComma) {
				CharAccumulator_appendChars(outerSource, ", ");
			}
			CharAccumulator_appendChars(outerSource, variable->LLVMtype);
			
			addComma = 1;
			
			currentPropertyBinding = currentPropertyBinding->next;
		}
	}
	
	CharAccumulator_appendChars(outerSource, " }");
}

int buildLLVM(ModuleInformation *MI, ContextBinding_function *outerFunction, CharAccumulator *outerSource, CharAccumulator *innerSource, linkedList_Node *expectedTypes, linkedList_Node **types, linkedList_Node *current, int loadVariables, int withTypes, int withCommas) {
	MI->level++;
	if (MI->level > maxContextLevel) {
		printf("level (%i) > maxContextLevel (%i)\n", MI->level, maxContextLevel);
		abort();
	}
	
	linkedList_Node **currentExpectedType = NULL;
	if (expectedTypes != NULL) {
		currentExpectedType = &expectedTypes;
	}
	
	int hasReturned = 0;
	
	//
	// pre-loop
	//
	
	if (MI->level == 0) {
		linkedList_Node *preLoopCurrent = current;
		while (preLoopCurrent != NULL) {
			ASTnode *node = ((ASTnode *)preLoopCurrent->data);
			
			if (node->nodeType == ASTnodeType_import) {
				ASTnode_import *data = (ASTnode_import *)node->value;
				
				if (data->path->length == 0) {
					addStringToReportMsg("empty import path");
					addStringToReportIndicator("import statements require a path");
					compileError(MI, node->location);
				}
				
				// TODO: hack to make "~" work on macOS
				char *path;
				if (data->path->start[0] == '~') {
					char *homePath = getenv("HOME");
					int pathSize = (int)strlen(homePath) + data->path->length;
					path = safeMalloc(pathSize);
					snprintf(path, pathSize, "%.*s%s", (int)strlen(homePath), homePath, data->path->start + 1);
				} else {
					int pathSize = (int)strlen(MI->path) + 1 + data->path->length + 1;
					path = safeMalloc(pathSize);
					snprintf(path, pathSize, "%.*s/%s", (int)strlen(MI->path), MI->path, data->path->start);
				}
				
				CharAccumulator *topLevelConstantSource = safeMalloc(sizeof(CharAccumulator));
				(*topLevelConstantSource) = (CharAccumulator){100, 0, 0};
				CharAccumulator_initialize(topLevelConstantSource);
				
				CharAccumulator *topLevelFunctionSource = safeMalloc(sizeof(CharAccumulator));
				(*topLevelFunctionSource) = (CharAccumulator){100, 0, 0};
				CharAccumulator_initialize(topLevelFunctionSource);
				
				CharAccumulator *LLVMmetadataSource = safeMalloc(sizeof(CharAccumulator));
				(*LLVMmetadataSource) = (CharAccumulator){100, 0, 0};
				CharAccumulator_initialize(LLVMmetadataSource);
				
				ModuleInformation *newMI = ModuleInformation_new(path, topLevelConstantSource, topLevelFunctionSource, LLVMmetadataSource);
				compileModule(newMI, CompilerMode_build_objectFile, path);
				
				linkedList_Node *current = newMI->context.bindings[0];
				
				while (current != NULL) {
					ContextBinding *binding = ((ContextBinding *)current->data);
					
					expectUnusedName(MI, binding->key, node->location);
					
					if (binding->type == ContextBindingType_struct) {
						generateStruct(MI, outerSource, binding, NULL, 0);
					} else if (binding->type == ContextBindingType_function) {
						ContextBinding_function *function = (ContextBinding_function *)binding->value;
						generateFunction(MI, outerSource, function, NULL, 0);
					}
					
					current = current->next;
				}
				
				// add the new module to importedModules
				ModuleInformation **modulePointerData = linkedList_addNode(&MI->context.importedModules, sizeof(void *));
				*modulePointerData = newMI;
			}
			
			else if (node->nodeType == ASTnodeType_struct) {
				ASTnode_struct *data = (ASTnode_struct *)node->value;
				
				ContextBinding *structBinding = linkedList_addNode(&MI->context.bindings[MI->level], sizeof(ContextBinding) + sizeof(ContextBinding_struct));
				structBinding->key = data->name;
				structBinding->type = ContextBindingType_struct;
				structBinding->byteSize = 0;
				// 4 by default but set it to pointer_byteSize if there is a pointer is in the struct
				structBinding->byteAlign = 4;
				
				int strlength = strlen("%struct.");
				
				char *LLVMname = safeMalloc(strlength + data->name->length + 1);
				memcpy(LLVMname, "%struct.", strlength);
				memcpy(LLVMname + strlength, data->name->start, data->name->length);
				LLVMname[strlength + data->name->length] = 0;
				
				((ContextBinding_struct *)structBinding->value)->LLVMname = LLVMname;
				((ContextBinding_struct *)structBinding->value)->propertyBindings = NULL;
				((ContextBinding_struct *)structBinding->value)->methodBindings = NULL;
				
				generateStruct(MI, outerSource, structBinding, node, 1);
			}
			
			else if (node->nodeType == ASTnodeType_function) {
				ASTnode_function *data = (ASTnode_function *)node->value;
				
				// make sure that the name is not already used
				expectUnusedName(MI, ((ASTnode_function *)node->value)->name, node->location);
				
				char *LLVMname = safeMalloc(data->name->length + 1);
				memcpy(LLVMname, data->name->start, data->name->length);
				LLVMname[data->name->length] = 0;
				
				addFunctionToList(LLVMname, MI, &MI->context.bindings[MI->level], node);
			}
			
			else if (node->nodeType == ASTnodeType_macro) {
				ASTnode_macro *data = (ASTnode_macro *)node->value;
				
				// make sure that the name is not already used
				expectUnusedName(MI, data->name, node->location);
				
				ContextBinding *macroData = linkedList_addNode(&MI->context.bindings[MI->level], sizeof(ContextBinding) + sizeof(ContextBinding_macro));
				
				macroData->key = data->name;
				macroData->type = ContextBindingType_macro;
				// I do not think I need to set byteSize or byteAlign to anything specific
				macroData->byteSize = 0;
				macroData->byteAlign = 0;
				
				((ContextBinding_macro *)macroData->value)->originModule = MI;
				((ContextBinding_macro *)macroData->value)->codeBlock = data->codeBlock;
			}
			
			preLoopCurrent = preLoopCurrent->next;
		}
	}
	
	//
	// main loop
	//
	
	while (current != NULL) {
		ASTnode *node = ((ASTnode *)current->data);
		
		switch (node->nodeType) {
			case ASTnodeType_import: {
				if (MI->level != 0) {
					addStringToReportMsg("import statements are only allowed at top level");
					compileError(MI, node->location);
				}
				
				break;
			}
			
			case ASTnodeType_constrainedType: {
				abort();
			}
			
			case ASTnodeType_struct: {
				if (MI->level != 0) {
					addStringToReportMsg("struct definitions are only allowed at top level");
					compileError(MI, node->location);
				}
				
				break;
			}
			
			case ASTnodeType_implement: {
				if (MI->level != 0) {
					addStringToReportMsg("implement can only be used at top level");
					compileError(MI, node->location);
				}
				
				ASTnode_implement *data = (ASTnode_implement *)node->value;
				
				// make sure the thing actually exists
				ContextBinding *bindingToImplement = NULL;
				int index = MI->level;
				while (1) {
					if (index < 0) {
						addStringToReportMsg("implement requires an identifier from this module");
						
						addStringToReportIndicator("nothing named '");
						addSubStringToReportIndicator(data->name);
						addStringToReportIndicator("'");
						compileError(MI, node->location);
					}
					linkedList_Node *current = MI->context.bindings[index];
					
					while (current != NULL) {
						ContextBinding *binding = ((ContextBinding *)current->data);
						
						if (SubString_SubString_cmp(binding->key, data->name) == 0) {
							bindingToImplement = binding;
							break;
						}
						
						current = current->next;
					}
					if (bindingToImplement != NULL) break;
					
					index--;
				}
				
				linkedList_Node *currentMethod = data->block;
				while (1) {
					if (currentMethod == NULL) {
						break;
					}
					
					ASTnode *methodNode = (ASTnode *)currentMethod->data;
					if (methodNode->nodeType != ASTnodeType_function) abort();
					ASTnode_function *methodData = (ASTnode_function *)methodNode->value;
					
					if (bindingToImplement->type == ContextBindingType_struct) {
						ContextBinding_struct *structToImplement = (ContextBinding_struct *)bindingToImplement->value;
						
						int LLVMnameSize = bindingToImplement->key->length + 1 + methodData->name->length + 1;
						char *LLVMname = safeMalloc(LLVMnameSize);
						snprintf(LLVMname, LLVMnameSize, "%.*s.%s", bindingToImplement->key->length, bindingToImplement->key->start, methodData->name->start);
						
						ContextBinding_function *function = (ContextBinding_function *)addFunctionToList(LLVMname, MI, &structToImplement->methodBindings, methodNode)->value;
						generateFunction(MI, MI->topLevelFunctionSource, function, methodNode, 1);
					} else {
						abort();
					}
					currentMethod = currentMethod->next;
				}
				
				break;
			}
			
			case ASTnodeType_function: {
				if (MI->level != 0) {
					addStringToReportMsg("function definitions are only allowed at top level");
					compileError(MI, node->location);
				}
				
				ASTnode_function *data = (ASTnode_function *)node->value;
				
				ContextBinding *functionBinding = getContextBindingFromSubString(MI, data->name);
				if (functionBinding == NULL || functionBinding->type != ContextBindingType_function) abort();
				ContextBinding_function *function = (ContextBinding_function *)functionBinding->value;
				
				generateFunction(MI, outerSource, function, node, 1);
				
				break;
			}
			
			case ASTnodeType_call: {
				if (outerFunction == NULL) {
					addStringToReportMsg("function calls are only allowed in a function");
					compileError(MI, node->location);
				}
				
				ASTnode_call *data = (ASTnode_call *)node->value;
				
				CharAccumulator leftSource = {100, 0, 0};
				CharAccumulator_initialize(&leftSource);
				
				linkedList_Node *expectedTypesForCall = NULL;
				addTypeFromString(MI, &expectedTypesForCall, "Function");
				
				linkedList_Node *leftTypes = NULL;
				buildLLVM(MI, outerFunction, outerSource, &leftSource, expectedTypesForCall, &leftTypes, data->left, 1, 0, 0);
				
				ContextBinding *functionToCallBinding = ((BuilderType *)leftTypes->data)->binding;
				ContextBinding_function *functionToCall = (ContextBinding_function *)functionToCallBinding->value;
				
				if (types != NULL) {
					addTypeFromBuilderType(MI, types, &functionToCall->returnType);
					break;
				}
				
				int expectedArgumentCount = linkedList_getCount(&functionToCall->argumentTypes);
				int actualArgumentCount = linkedList_getCount(&data->arguments);
				
				if (expectedArgumentCount > actualArgumentCount) {
					SubString_print(functionToCallBinding->key);
					printf(" did not get enough arguments (expected %d but got %d)\n", expectedArgumentCount, actualArgumentCount);
					compileError(MI, node->location);
				}
				if (expectedArgumentCount < actualArgumentCount) {
					SubString_print(functionToCallBinding->key);
					printf(" got too many arguments (expected %d but got %d)\n", expectedArgumentCount, actualArgumentCount);
					compileError(MI, node->location);
				}
				
				if (expectedTypes != NULL) {
					expectType(MI, NULL, (BuilderType *)expectedTypes->data, &functionToCall->returnType, node->location);
				}
				
				if (types != NULL) {
					addTypeFromBuilderType(MI, types, &functionToCall->returnType);
				} else {
					CharAccumulator newInnerSource = {100, 0, 0};
					CharAccumulator_initialize(&newInnerSource);
					
					buildLLVM(MI, outerFunction, outerSource, &newInnerSource, functionToCall->argumentTypes, NULL, data->arguments, 1, 1, 1);
					
					if (strcmp(functionToCall->LLVMreturnType, "void") != 0) {
						CharAccumulator_appendChars(outerSource, "\n\t%");
						CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
						CharAccumulator_appendChars(outerSource, " = call ");
						
						if (innerSource != NULL) {
							if (withTypes) {
								CharAccumulator_appendChars(innerSource, functionToCall->LLVMreturnType);
								CharAccumulator_appendChars(innerSource, " ");
							}
							CharAccumulator_appendChars(innerSource, "%");
							CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
						}
						
						outerFunction->registerCount++;
					} else {
						CharAccumulator_appendChars(outerSource, "\n\tcall ");
					}
					CharAccumulator_appendChars(outerSource, functionToCall->LLVMreturnType);
					CharAccumulator_appendChars(outerSource, " @");
					CharAccumulator_appendChars(outerSource, functionToCall->LLVMname);
					CharAccumulator_appendChars(outerSource, "(");
					CharAccumulator_appendChars(outerSource, newInnerSource.data);
					CharAccumulator_appendChars(outerSource, ")");
					
					if (compilerOptions.includeDebugInformation) {
						addDILocation(outerSource, outerFunction->debugInformationScopeID, node->location);
					}
					
					CharAccumulator_free(&newInnerSource);
				}
				
				CharAccumulator_free(&leftSource);
				
				break;
			}
			
			case ASTnodeType_macro: {
				break;
			}
			
			case ASTnodeType_runMacro: {
				ASTnode_runMacro *data = (ASTnode_runMacro *)node->value;
				
				linkedList_Node *expectedTypesForRunMacro = NULL;
				addTypeFromString(MI, &expectedTypesForRunMacro, "__Macro");
				
				linkedList_Node *leftTypes = NULL;
				buildLLVM(MI, outerFunction, outerSource, NULL, expectedTypesForRunMacro, &leftTypes, data->left, 1, 0, 0);
				
				ContextBinding *macroToRunBinding = ((BuilderType *)leftTypes->data)->binding;
				if (macroToRunBinding == NULL) {
					addStringToReportMsg("macro does not exist");
					compileError(MI, node->location);
				}
				ContextBinding_macro *macroToRun = (ContextBinding_macro *)macroToRunBinding->value;
				
				if (macroToRun->originModule->name != NULL && strcmp(macroToRun->originModule->name, "__core__") == 0) {
					// from the __core__ module, so this is a special case
					if (SubString_string_cmp(macroToRunBinding->key, "error") == 0) {
						int argumentCount = linkedList_getCount(&data->arguments);
						if (argumentCount != 2) {
							addStringToReportMsg("#error(message:String, indicator:String) expected 2 arguments");
							compileError(MI, node->location);
						}
						
						ASTnode *message = (ASTnode *)data->arguments->data;
						if (message->nodeType != ASTnodeType_string) {
							addStringToReportMsg("message must be a string");
							compileError(MI, message->location);
						}
						
						ASTnode *indicator = (ASTnode *)data->arguments->next->data;
						if (indicator->nodeType != ASTnodeType_string) {
							addStringToReportMsg("indicator must be a string");
							compileError(MI, indicator->location);
						}
						
						addSubStringToReportMsg(((ASTnode_string *)message->value)->value);
						addSubStringToReportIndicator(((ASTnode_string *)indicator->value)->value);
						compileError(MI, node->location);
					} else if (SubString_string_cmp(macroToRunBinding->key, "describe") == 0) {
						int argumentCount = linkedList_getCount(&data->arguments);
						if (argumentCount != 1) {
							addStringToReportMsg("#error(variable) expected 1 argument");
							compileError(MI, node->location);
						}
						
						ContextBinding *variableBinding = getContextBindingFromIdentifierNode(MI, (ASTnode *)data->arguments->data);
						
						CharAccumulator variableDescription = {100, 0, 0};
						CharAccumulator_initialize(&variableDescription);
						
						getVariableDescription(MI, &variableDescription, variableBinding);
						
						printf("%.*s", (int)variableDescription.size, variableDescription.data);
						
						CharAccumulator_free(&variableDescription);
					} else {
						abort();
					}
				} else {
					// TODO: remove hack?
					ModuleContext originalModuleContext = MI->context;
					MI->context = macroToRun->originModule->context;
					buildLLVM(MI, outerFunction, outerSource, NULL, NULL, NULL, macroToRun->codeBlock, 0, 0, 0);
					MI->context = originalModuleContext;
				}
				break;
			}
				
			case ASTnodeType_while: {
				if (outerFunction == NULL) {
					addStringToReportMsg("while loops are only allowed in a function");
					compileError(MI, node->location);
				}
				
				ASTnode_while *data = (ASTnode_while *)node->value;
				
				linkedList_Node *expectedTypesForWhile = NULL;
				addTypeFromString(MI, &expectedTypesForWhile, "Bool");
				
				CharAccumulator expressionSource = {100, 0, 0};
				CharAccumulator_initialize(&expressionSource);
				
				int jump1 = outerFunction->registerCount;
				outerFunction->registerCount++;
				CharAccumulator jump1_outerSource = {100, 0, 0};
				CharAccumulator_initialize(&jump1_outerSource);
				buildLLVM(MI, outerFunction, &jump1_outerSource, &expressionSource, expectedTypesForWhile, NULL, data->expression, 1, 1, 0);
				
				int jump2 = outerFunction->registerCount;
				outerFunction->registerCount++;
				CharAccumulator jump2_outerSource = {100, 0, 0};
				CharAccumulator_initialize(&jump2_outerSource);
				buildLLVM(MI, outerFunction, &jump2_outerSource, NULL, NULL, NULL, data->codeBlock, 0, 0, 0);
				
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
					compileError(MI, node->location);
				}
				
				ASTnode_if *data = (ASTnode_if *)node->value;
				
				linkedList_Node *expectedTypesForIf = NULL;
				addTypeFromString(MI, &expectedTypesForIf, "Bool");
				
				CharAccumulator expressionSource = {100, 0, 0};
				CharAccumulator_initialize(&expressionSource);
				
				linkedList_Node *types = NULL;
				buildLLVM(MI, outerFunction, outerSource, &expressionSource, expectedTypesForIf, &types, data->expression, 1, 1, 0);
				
				int typesCount = linkedList_getCount(&types);
				
				if (typesCount == 0) {
					addStringToReportMsg("if statement condition expected a bool but got nothing");
					
					if (data->expression == NULL) {
						addStringToReportIndicator("if statement condition is empty");
					} else if (((ASTnode_operator *)((ASTnode *)data->expression->data)->value)->operatorType == ASTnode_operatorType_assignment) {
						addStringToReportIndicator("did you mean to use two equals?");
					}
					
					compileError(MI, node->location);
				} else if (typesCount > 1) {
					addStringToReportMsg("if statement condition got more than one type");
					
					compileError(MI, node->location);
				}
				
				if (((ASTnode *)data->expression->data)->nodeType == ASTnodeType_operator) {
					applyFacts(MI, (ASTnode_operator *)((ASTnode *)data->expression->data)->value);
				}
				
				int jump1 = outerFunction->registerCount;
				outerFunction->registerCount++;
				CharAccumulator trueCodeBlockSource = {100, 0, 0};
				CharAccumulator_initialize(&trueCodeBlockSource);
				int trueHasReturned = buildLLVM(MI, outerFunction, &trueCodeBlockSource, NULL, NULL, NULL, data->trueCodeBlock, 0, 0, 0);
				CharAccumulator falseCodeBlockSource = {100, 0, 0};
				CharAccumulator_initialize(&falseCodeBlockSource);
				
				int endJump;
				
				if (data->falseCodeBlock == NULL) {
					endJump = outerFunction->registerCount;
					outerFunction->registerCount += 2;
					
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
					int falseHasReturned = buildLLVM(MI, outerFunction, &falseCodeBlockSource, NULL, NULL, NULL, data->falseCodeBlock, 0, 0, 0);
					
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
				linkedList_freeList(&types);
				
				break;
			}
			
			case ASTnodeType_return: {
				if (outerFunction == NULL) {
					addStringToReportMsg("return statements are only allowed in a function");
					compileError(MI, node->location);
				}
				
				ASTnode_return *data = (ASTnode_return *)node->value;
				
				if (data->expression == NULL) {
					if (!ifTypeIsNamed(&outerFunction->returnType, "Void")) {
						addStringToReportMsg("returning Void in a function that does not return Void.");
						compileError(MI, node->location);
					}
					CharAccumulator_appendChars(outerSource, "\n\tret void");
				} else {
					CharAccumulator newInnerSource = {100, 0, 0};
					CharAccumulator_initialize(&newInnerSource);
					
					buildLLVM(MI, outerFunction, outerSource, &newInnerSource, typeToList(outerFunction->returnType), NULL, data->expression, 1, 1, 0);
					
					CharAccumulator_appendChars(outerSource, "\n\tret ");
					
					CharAccumulator_appendChars(outerSource, newInnerSource.data);
					
					CharAccumulator_free(&newInnerSource);
				}
				
				if (compilerOptions.includeDebugInformation) {
					addDILocation(outerSource, outerFunction->debugInformationScopeID, node->location);
				}
				
				hasReturned = 1;
				
				outerFunction->registerCount++;
				
				break;
			}
				
			case ASTnodeType_variableDefinition: {
				if (outerFunction == NULL) {
					addStringToReportMsg("variable definitions are only allowed in a function");
					compileError(MI, node->location);
				}
				
				ASTnode_variableDefinition *data = (ASTnode_variableDefinition *)node->value;
				
				expectUnusedName(MI, data->name, node->location);
				
				BuilderType* type = getType(MI, data->type);
				
				char *LLVMtype = getLLVMtypeFromBinding(MI, type->binding);
				
				CharAccumulator expressionSource = {100, 0, 0};
				CharAccumulator_initialize(&expressionSource);
				
				buildLLVM(MI, outerFunction, outerSource, &expressionSource, typeToList(*type), NULL, data->expression, 1, 1, 0);
				
				CharAccumulator_appendChars(outerSource, "\n\t%");
				CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
				CharAccumulator_appendChars(outerSource, " = alloca ");
				CharAccumulator_appendChars(outerSource, LLVMtype);
				CharAccumulator_appendChars(outerSource, ", align ");
				CharAccumulator_appendInt(outerSource, type->binding->byteAlign);
				
				if (data->expression != NULL) {
					CharAccumulator_appendChars(outerSource, "\n\tstore ");
					CharAccumulator_appendChars(outerSource, expressionSource.data);
					CharAccumulator_appendChars(outerSource, ", ptr %");
					CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, ", align ");
					CharAccumulator_appendInt(outerSource, type->binding->byteAlign);
				}
				
				ContextBinding *variableBinding = linkedList_addNode(&MI->context.bindings[MI->level], sizeof(ContextBinding) + sizeof(ContextBinding_variable));
				
				variableBinding->key = data->name;
				variableBinding->type = ContextBindingType_variable;
				variableBinding->byteSize = pointer_byteSize;
				variableBinding->byteAlign = pointer_byteSize;
				
				((ContextBinding_variable *)variableBinding->value)->LLVMRegister = outerFunction->registerCount;
				((ContextBinding_variable *)variableBinding->value)->LLVMtype = LLVMtype;
				((ContextBinding_variable *)variableBinding->value)->type = *type;
				memset(((ContextBinding_variable *)variableBinding->value)->factStack, 0, sizeof(linkedList_Node *[maxContextLevel]));
				
				outerFunction->registerCount++;
				
				CharAccumulator_free(&expressionSource);
				
				break;
			}
			
			case ASTnodeType_operator: {
				ASTnode_operator *data = (ASTnode_operator *)node->value;
				
				CharAccumulator leftInnerSource = {100, 0, 0};
				CharAccumulator_initialize(&leftInnerSource);
				
				CharAccumulator rightInnerSource = {100, 0, 0};
				CharAccumulator_initialize(&rightInnerSource);
				
				if (data->operatorType == ASTnode_operatorType_assignment) {
					if (outerFunction == NULL) {
						addStringToReportMsg("variable assignments are only allowed in a function");
						compileError(MI, node->location);
					}
					
					ContextBinding *leftVariable = getContextBindingFromIdentifierNode(MI, (ASTnode *)data->left->data);
					
					CharAccumulator leftSource = {100, 0, 0};
					CharAccumulator_initialize(&leftSource);
					linkedList_Node *leftType = NULL;
					buildLLVM(MI, outerFunction, outerSource, &leftSource, NULL, &leftType, data->left, 0, 0, 0);
					
					CharAccumulator rightSource = {100, 0, 0};
					CharAccumulator_initialize(&rightSource);
					buildLLVM(MI, outerFunction, outerSource, &rightSource, leftType, NULL, data->right, 1, 1, 0);
					
					CharAccumulator_appendChars(outerSource, "\n\tstore ");
					CharAccumulator_appendChars(outerSource, rightSource.data);
					CharAccumulator_appendChars(outerSource, ", ptr ");
					CharAccumulator_appendChars(outerSource, leftSource.data);
					CharAccumulator_appendChars(outerSource, ", align ");
					CharAccumulator_appendInt(outerSource, leftVariable->byteAlign);
					
					CharAccumulator_free(&rightSource);
					CharAccumulator_free(&leftSource);
					
					break;
				}
				
				else if (data->operatorType == ASTnode_operatorType_memberAccess) {
					ASTnode *rightNode = (ASTnode *)data->right->data;
					
					if (rightNode->nodeType != ASTnodeType_identifier) {
						addStringToReportMsg("right side of memberAccess must be an identifier");
						
						addStringToReportIndicator("right side of memberAccess is not an identifier");
						compileError(MI, rightNode->location);
					};
					
					linkedList_Node *leftTypes = NULL;
					buildLLVM(MI, outerFunction, outerSource, &leftInnerSource, NULL, &leftTypes, data->left, 0, 0, 0);
					
					ContextBinding *structBinding = ((BuilderType *)leftTypes->data)->binding;
					if (structBinding->type != ContextBindingType_struct) {
						addStringToReportMsg("left side of member access is not a struct");
						compileError(MI, ((ASTnode *)data->left->data)->location);
					}
					ContextBinding_struct *structData = (ContextBinding_struct *)structBinding->value;
					
					ASTnode_identifier *rightData = (ASTnode_identifier *)rightNode->value;
					
					int index = 0;
					
					linkedList_Node *currentPropertyBinding = structData->propertyBindings;
					while (1) {
						if (currentPropertyBinding == NULL) {
							break;
						}
						
						ContextBinding *propertyBinding = (ContextBinding *)currentPropertyBinding->data;
						
						if (SubString_SubString_cmp(propertyBinding->key, rightData->name) == 0) {
							if (propertyBinding->type == ContextBindingType_variable) {
								if (types != NULL) {
									addTypeFromBuilderType(MI, types, &((ContextBinding_variable *)propertyBinding->value)->type);
								}
								
								CharAccumulator_appendChars(outerSource, "\n\t%");
								CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
								CharAccumulator_appendChars(outerSource, " = getelementptr inbounds ");
								CharAccumulator_appendChars(outerSource, structData->LLVMname);
								CharAccumulator_appendChars(outerSource, ", ptr ");
								CharAccumulator_appendChars(outerSource, leftInnerSource.data);
								CharAccumulator_appendChars(outerSource, ", i32 0");
								CharAccumulator_appendChars(outerSource, ", i32 ");
								CharAccumulator_appendInt(outerSource, index);
								
								if (loadVariables) {
									CharAccumulator_appendChars(outerSource, "\n\t%");
									CharAccumulator_appendInt(outerSource, outerFunction->registerCount + 1);
									CharAccumulator_appendChars(outerSource, " = load ");
									CharAccumulator_appendChars(outerSource, ((ContextBinding_variable *)propertyBinding->value)->LLVMtype);
									CharAccumulator_appendChars(outerSource, ", ptr %");
									CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
									CharAccumulator_appendChars(outerSource, ", align ");
									CharAccumulator_appendInt(outerSource, propertyBinding->byteAlign);
									
									if (withTypes) {
										CharAccumulator_appendChars(innerSource, ((ContextBinding_variable *)propertyBinding->value)->LLVMtype);
										CharAccumulator_appendChars(innerSource, " ");
									}
									
									outerFunction->registerCount++;
								}
								
								if (innerSource) {
									CharAccumulator_appendChars(innerSource, "%");
									CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
								} else {
									addStringToReportMsg("unused member access");
									compileWarning(MI, rightNode->location, WarningType_unused);
								}
								
								outerFunction->registerCount++;
							} else {
								abort();
							}
							
							break;
						}
						
						currentPropertyBinding = currentPropertyBinding->next;
						if (propertyBinding->type != ContextBindingType_function) {
							index++;
						}
					}
					
					linkedList_Node *currentMethodBinding = structData->methodBindings;
					while (1) {
						if (currentMethodBinding == NULL) {
							break;
						}
						
						ContextBinding *methodBinding = (ContextBinding *)currentMethodBinding->data;
						if (methodBinding->type != ContextBindingType_function) abort();
						
						if (SubString_SubString_cmp(methodBinding->key, rightData->name) == 0) {
							if (types != NULL) {
								addTypeFromBinding(MI, types, methodBinding);
							}
							break;
						}
						
						currentMethodBinding = currentMethodBinding->next;
					}
					
					if (currentPropertyBinding == NULL && currentMethodBinding == NULL) {
						addStringToReportMsg("left side of memberAccess must exist");
						compileError(MI, ((ASTnode *)data->left->data)->location);
					}
					
					CharAccumulator_free(&leftInnerSource);
					CharAccumulator_free(&rightInnerSource);
					break;
				}
				
				else if (data->operatorType == ASTnode_operatorType_scopeResolution) {
					ASTnode *leftNode = (ASTnode *)data->left->data;
					ASTnode *rightNode = (ASTnode *)data->right->data;
					
					// for now just assume everything is an identifier
					if (leftNode->nodeType != ASTnodeType_identifier) abort();
					if (rightNode->nodeType != ASTnodeType_identifier) abort();
					
					SubString *leftSubString = ((ASTnode_identifier *)leftNode->value)->name;
					SubString *rightSubString = ((ASTnode_identifier *)rightNode->value)->name;
					
					// find the module
					linkedList_Node *currentModule = MI->context.importedModules;
					while (1) {
						if (currentModule == NULL) {
							addStringToReportMsg("not an imported module");
							
							addStringToReportIndicator("no imported module named '");
							addSubStringToReportIndicator(leftSubString);
							addStringToReportIndicator("'");
							compileError(MI, leftNode->location);
						}
						ModuleInformation *moduleInformation = *(ModuleInformation **)currentModule->data;
						
						if (SubString_string_cmp(leftSubString, moduleInformation->name) != 0) {
							currentModule = currentModule->next;
							continue;
						}
						
						linkedList_Node *current = moduleInformation->context.bindings[0];
						
						while (1) {
							if (current == NULL) {
								addStringToReportMsg("value does not exist in this modules scope");
								
								addStringToReportIndicator("nothing in module '");
								addSubStringToReportIndicator(leftSubString);
								addStringToReportIndicator("' with name '");
								addSubStringToReportIndicator(rightSubString);
								addStringToReportIndicator("'");
								compileError(MI, rightNode->location);
							}
							ContextBinding *binding = ((ContextBinding *)current->data);
							
							if (SubString_SubString_cmp(binding->key, rightSubString) == 0) {
								// success
								addTypeFromBinding(MI, types, binding);
								break;
							}
							
							current = current->next;
						}
						
						break;
					}
					
					break;
				}
				
				if (expectedTypes == NULL) {
					addStringToReportMsg("unexpected operator");
					
					addStringToReportIndicator("the return value of this operator is not used");
					compileError(MI, node->location);
				}
				
				linkedList_Node *expectedTypeForLeftAndRight = NULL;
				char *expectedLLVMtype = getLLVMtypeFromBinding(MI, ((BuilderType *)expectedTypes->data)->binding);
				
				// all of these operators are very similar and even use the same 'icmp' instruction
				// https://llvm.org/docs/LangRef.html#fcmp-instruction
				if (
					data->operatorType == ASTnode_operatorType_equivalent ||
					data->operatorType == ASTnode_operatorType_greaterThan ||
					data->operatorType == ASTnode_operatorType_lessThan
				) {
					//
					// figure out what the type of both sides should be
					//
					
					buildLLVM(MI, outerFunction, NULL, NULL, NULL, &expectedTypeForLeftAndRight, data->left, 0, 0, 0);
					if (
						!ifTypeIsNamed((BuilderType *)expectedTypeForLeftAndRight->data, "Int8") &&
						!ifTypeIsNamed((BuilderType *)expectedTypeForLeftAndRight->data, "Int32") &&
						!ifTypeIsNamed((BuilderType *)expectedTypeForLeftAndRight->data, "Int64")
					) {
						if (
							!ifTypeIsNamed((BuilderType *)expectedTypeForLeftAndRight->data, "__Number")
//							ifTypeIsNamed((BuilderType *)expectedTypeForLeftAndRight->data, "Pointer")
						) {
							addStringToReportMsg("left side of operator expected a number");
							compileError(MI, node->location);
						}
						
						linkedList_freeList(&expectedTypeForLeftAndRight);
						buildLLVM(MI, outerFunction, NULL, NULL, NULL, &expectedTypeForLeftAndRight, data->right, 0, 0, 0);
						if (
							!ifTypeIsNamed((BuilderType *)expectedTypeForLeftAndRight->data, "Int8") &&
							!ifTypeIsNamed((BuilderType *)expectedTypeForLeftAndRight->data, "Int32") &&
							!ifTypeIsNamed((BuilderType *)expectedTypeForLeftAndRight->data, "Int64")
						) {
							if (
								!ifTypeIsNamed((BuilderType *)expectedTypeForLeftAndRight->data, "__Number")
								// ifTypeIsNamed((BuilderType *)expectedTypeForLeftAndRight->data, "Pointer")
							) {
								addStringToReportMsg("right side of operator expected a number");
								compileError(MI, node->location);
							}
							
							// default to Int32
							linkedList_freeList(&expectedTypeForLeftAndRight);
							addTypeFromString(MI, &expectedTypeForLeftAndRight, "Int32");
						}
					}
					
					//
					// build both sides
					//
					
					buildLLVM(MI, outerFunction, outerSource, &leftInnerSource, expectedTypeForLeftAndRight, NULL, data->left, 1, 0, 0);
					buildLLVM(MI, outerFunction, outerSource, &rightInnerSource, expectedTypeForLeftAndRight, NULL, data->right, 1, 0, 0);
					CharAccumulator_appendChars(outerSource, "\n\t%");
					CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, " = icmp ");
					
					if (data->operatorType == ASTnode_operatorType_equivalent) {
						// eq: yields true if the operands are equal, false otherwise. No sign interpretation is necessary or performed.
						CharAccumulator_appendChars(outerSource, "eq ");
					}
					
					else if (data->operatorType == ASTnode_operatorType_greaterThan) {
						// sgt: interprets the operands as signed values and yields true if op1 is greater than op2.
						CharAccumulator_appendChars(outerSource, "sgt ");
					}
					
					else if (data->operatorType == ASTnode_operatorType_lessThan) {
						// slt: interprets the operands as signed values and yields true if op1 is less than op2.
						CharAccumulator_appendChars(outerSource, "slt ");
					}
					
					else {
						abort();
					}
					
					if (types != NULL) {
						addTypeFromString(MI, types, "Bool");
					}
					
					CharAccumulator_appendChars(outerSource, getLLVMtypeFromBinding(MI, ((BuilderType *)expectedTypeForLeftAndRight->data)->binding));
				}
				
				// +
				else if (data->operatorType == ASTnode_operatorType_add) {
					// the expected type for both sides of the operator is the same type that is expected for the operator
					addTypeFromBuilderType(MI, &expectedTypeForLeftAndRight, (BuilderType *)expectedTypes->data);
					
					buildLLVM(MI, outerFunction, outerSource, &leftInnerSource, expectedTypeForLeftAndRight, NULL, data->left, 1, 0, 0);
					buildLLVM(MI, outerFunction, outerSource, &rightInnerSource, expectedTypeForLeftAndRight, NULL, data->right, 1, 0, 0);
					CharAccumulator_appendChars(outerSource, "\n\t%");
					CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, " = add nsw ");
					CharAccumulator_appendChars(outerSource, expectedLLVMtype);
				}
				
				// -
				else if (data->operatorType == ASTnode_operatorType_subtract) {
					// the expected type for both sides of the operator is the same type that is expected for the operator
					addTypeFromBuilderType(MI, &expectedTypeForLeftAndRight, (BuilderType *)expectedTypes->data);
					
					buildLLVM(MI, outerFunction, outerSource, &leftInnerSource, expectedTypeForLeftAndRight, NULL, data->left, 1, 0, 0);
					buildLLVM(MI, outerFunction, outerSource, &rightInnerSource, expectedTypeForLeftAndRight, NULL, data->right, 1, 0, 0);
					CharAccumulator_appendChars(outerSource, "\n\t%");
					CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, " = sub nsw ");
					CharAccumulator_appendChars(outerSource, expectedLLVMtype);
				}
				
				else {
					abort();
				}
				CharAccumulator_appendChars(outerSource, " ");
				CharAccumulator_appendChars(outerSource, leftInnerSource.data);
				CharAccumulator_appendChars(outerSource, ", ");
				CharAccumulator_appendChars(outerSource, rightInnerSource.data);
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, expectedLLVMtype);
					CharAccumulator_appendChars(innerSource, " ");
				}
				CharAccumulator_appendChars(innerSource, "%");
				CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
				
				CharAccumulator_free(&leftInnerSource);
				CharAccumulator_free(&rightInnerSource);
				
				linkedList_freeList(&expectedTypeForLeftAndRight);
				
				outerFunction->registerCount++;
				
				break;
			}
			
			case ASTnodeType_true: {
				if (!ifTypeIsNamed((BuilderType *)expectedTypes->data, "Bool")) {
					addStringToReportMsg("unexpected type");
					
					addStringToReportIndicator("expected type '");
					addSubStringToReportIndicator(((BuilderType *)expectedTypes->data)->binding->key);
					addStringToReportIndicator("' but got a bool");
					compileError(MI, node->location);
				}
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, "i1 ");
				}
				
				CharAccumulator_appendChars(innerSource, "true");
				
				if (types != NULL) {
					addTypeFromString(MI, types, "Bool");
				}
				
				break;
			}
			
			case ASTnodeType_false: {
				if (!ifTypeIsNamed((BuilderType *)expectedTypes->data, "Bool")) {
					addStringToReportMsg("unexpected type");
					
					addStringToReportIndicator("expected type '");
					addSubStringToReportIndicator(((BuilderType *)expectedTypes->data)->binding->key);
					addStringToReportIndicator("' but got a bool");
					compileError(MI, node->location);
				}
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, "i1 ");
				}
				
				CharAccumulator_appendChars(innerSource, "false");
				
				if (types != NULL) {
					addTypeFromString(MI, types, "Bool");
				}
				
				break;
			}
				
			case ASTnodeType_number: {
				ASTnode_number *data = (ASTnode_number *)node->value;
				
				if (types != NULL) {
					addTypeFromString(MI, types, "__Number");
					break;
				}
				
				if (
					!ifTypeIsNamed((BuilderType *)expectedTypes->data, "Int8") &&
					!ifTypeIsNamed((BuilderType *)expectedTypes->data, "Int16") &&
					!ifTypeIsNamed((BuilderType *)expectedTypes->data, "Int32") &&
					!ifTypeIsNamed((BuilderType *)expectedTypes->data, "Int64")
				) {
					addStringToReportMsg("unexpected type");
					
					addStringToReportIndicator("expected type '");
					addSubStringToReportIndicator(getTypeAsSubString((BuilderType *)expectedTypes->data));
					addStringToReportIndicator("' but got a number.");
					compileError(MI, node->location);
				} else {
					ContextBinding *typeBinding = ((BuilderType *)expectedTypes->data)->binding;
					
					if (data->value > pow(2, (typeBinding->byteSize * 8) - 1) - 1) {
						addStringToReportMsg("integer overflow detected");
						
						CharAccumulator_appendInt(&reportIndicator, data->value);
						addStringToReportIndicator(" is larger than the maximum size of the type '");
						addSubStringToReportIndicator(getTypeAsSubString((BuilderType *)expectedTypes->data));
						addStringToReportIndicator("'");
						compileError(MI, node->location);
					}
				}
				
				char *LLVMtype = getLLVMtypeFromBinding(MI, ((BuilderType *)(*currentExpectedType)->data)->binding);
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, LLVMtype);
					CharAccumulator_appendChars(innerSource, " ");
				}
				
				CharAccumulator_appendSubString(innerSource, data->string);
				
				break;
			}
				
			case ASTnodeType_string: {
				ASTnode_string *data = (ASTnode_string *)node->value;
				
				if (!ifTypeIsNamed((BuilderType *)expectedTypes->data, "Pointer")) {
					addStringToReportMsg("unexpected type");
					
					addStringToReportIndicator("expected type '");
					addSubStringToReportIndicator(((BuilderType *)expectedTypes->data)->binding->key);
					addStringToReportIndicator("' but got a string");
					compileError(MI, node->location);
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
							compileError(MI, (SourceLocation){node->location.line, node->location.columnStart + i + 1, node->location.columnEnd - (data->value->length - i)});
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
				
				CharAccumulator_appendChars(MI->topLevelConstantSource, "\n@.str.");
				CharAccumulator_appendInt(MI->topLevelConstantSource, MI->stringCount);
				CharAccumulator_appendChars(MI->topLevelConstantSource, " = private unnamed_addr constant [");
				CharAccumulator_appendInt(MI->topLevelConstantSource, stringLength);
				CharAccumulator_appendChars(MI->topLevelConstantSource, " x i8] c\"");
				CharAccumulator_appendChars(MI->topLevelConstantSource, string.data);
				CharAccumulator_appendChars(MI->topLevelConstantSource, "\\00\""); // the \00 is the NULL byte
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, "ptr ");
				}
				CharAccumulator_appendChars(innerSource, "@.str.");
				CharAccumulator_appendInt(innerSource, MI->stringCount);
				
				MI->stringCount++;
				
				CharAccumulator_free(&string);
				
				break;
			}
			
			case ASTnodeType_identifier: {
				ASTnode_identifier *data = (ASTnode_identifier *)node->value;
				
				ContextBinding *variableBinding = getContextBindingFromIdentifierNode(MI, node);
				if (variableBinding == NULL) {
					addStringToReportMsg("value not in scope");
					
					addStringToReportIndicator("nothing named '");
					addSubStringToReportIndicator(data->name);
					addStringToReportIndicator("'");
					compileError(MI, node->location);
				}
				
				if (
					variableBinding->type == ContextBindingType_simpleType ||
					variableBinding->type == ContextBindingType_struct
				) {
					if (types != NULL) {
						addTypeFromBinding(MI, types, variableBinding);
					}
				} else if (variableBinding->type == ContextBindingType_variable) {
					ContextBinding_variable *variable = (ContextBinding_variable *)variableBinding->value;
					
					if (expectedTypes != NULL) {
						expectType(MI, variable, (BuilderType *)expectedTypes->data, &variable->type, node->location);
					}
					
					if (types != NULL) {
						addTypeFromBuilderType(MI, types, &variable->type);
					}
					
					if (outerSource != NULL) {
						if (withTypes) {
							CharAccumulator_appendChars(innerSource, variable->LLVMtype);
							CharAccumulator_appendChars(innerSource, " ");
						}
						
						CharAccumulator_appendChars(innerSource, "%");
						
						if (loadVariables) {
							CharAccumulator_appendChars(outerSource, "\n\t%");
							CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
							CharAccumulator_appendChars(outerSource, " = load ");
							CharAccumulator_appendChars(outerSource, variable->LLVMtype);
							CharAccumulator_appendChars(outerSource, ", ptr %");
							CharAccumulator_appendInt(outerSource, variable->LLVMRegister);
							CharAccumulator_appendChars(outerSource, ", align ");
							CharAccumulator_appendInt(outerSource, variableBinding->byteAlign);
							
							CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
							
							outerFunction->registerCount++;
						} else {
							CharAccumulator_appendInt(innerSource, variable->LLVMRegister);
						}
					}
				} else if (variableBinding->type == ContextBindingType_function) {
					if (expectedTypes != NULL) {
						if (!ifTypeIsNamed((BuilderType *)expectedTypes->data, "Function")) {
							addStringToReportMsg("unexpected type");
							
							addStringToReportIndicator("expected type '");
							addSubStringToReportIndicator(((BuilderType *)expectedTypes->data)->binding->key);
							addStringToReportIndicator("' but got a function");
							compileError(MI, node->location);
						}
					}
					
					if (types != NULL) {
						addTypeFromBinding(MI, types, variableBinding);
					}
				} else if (variableBinding->type == ContextBindingType_macro) {
					if (expectedTypes != NULL) {
						if (!ifTypeIsNamed((BuilderType *)expectedTypes->data, "__Macro")) {
							addStringToReportMsg("unexpected type");
							
							addStringToReportIndicator("expected type '");
							addSubStringToReportIndicator(((BuilderType *)expectedTypes->data)->binding->key);
							addStringToReportIndicator("' but got a macro");
							compileError(MI, node->location);
						}
					}
					
					if (types != NULL) {
						addTypeFromBinding(MI, types, variableBinding);
					}
				} else {
					abort();
				}
				
				break;
			}
			
			default: {
				printf("unknown node type: %u\n", node->nodeType);
				compileError(MI, node->location);
				break;
			}
		}
		
		if (withCommas && current->next != NULL) {
			CharAccumulator_appendChars(innerSource, ",");
		}
		
		current = current->next;
		if (currentExpectedType != NULL) {
			*currentExpectedType = (*currentExpectedType)->next;
		}
	}
	
	if (MI->level != 0) {
		linkedList_freeList(&MI->context.bindings[MI->level]);
	}
	
	MI->level--;
	
	return hasReturned;
}

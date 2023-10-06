/*
 This file has the buildLLVM function and a bunch of functions that the BuildLLVM function calls.
 
 This file is: very big, hard to read, very hard to debug.
 Part of the debuging issue comes from how much casting is being used.
 But I do not think there is much I can do about that in C.
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
		
		// more recursion!
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
}

void addTypeFromBuilderType(ModuleInformation *MI, linkedList_Node **list, BuilderType *type) {
	BuilderType *data = linkedList_addNode(list, sizeof(BuilderType));
	data->binding = type->binding;
}

void addTypeFromBinding(ModuleInformation *MI, linkedList_Node **list, ContextBinding *binding) {
	BuilderType *data = linkedList_addNode(list, sizeof(BuilderType));
	data->binding = binding;
}

linkedList_Node *typeToList(BuilderType type) {
	linkedList_Node *list = NULL;
	
	BuilderType *data = linkedList_addNode(&list, sizeof(BuilderType));
	data->binding = type.binding;
	
	return list;
}

void expectSameBinding(ModuleInformation *MI, ContextBinding *expectedTypeBinding, ContextBinding *actualTypeBinding, SourceLocation location) {
	if (expectedTypeBinding != actualTypeBinding) {
		addStringToReportMsg("unexpected type");
		
		addStringToReportIndicator("expected type '");
		addSubStringToReportIndicator(expectedTypeBinding->key);
		addStringToReportIndicator("' but got type '");
		addSubStringToReportIndicator(actualTypeBinding->key);
		addStringToReportIndicator("'");
		compileError(MI, location);
	}
}

void expectUnusedMemberBindingName(ModuleInformation *MI, ContextBinding_struct *structData, SubString *name, SourceLocation location) {
	linkedList_Node *currentMemberBinding = structData->memberBindings;
	while (currentMemberBinding != NULL) {
		ContextBinding *memberBinding = (ContextBinding *)currentMemberBinding->data;
		
		if (SubString_SubString_cmp(memberBinding->key, name) == 0) {
			addStringToReportMsg("the name '");
			addSubStringToReportMsg(name);
			addStringToReportMsg("' is defined multiple times inside a struct");
			
//			addStringToReportIndicator("'");
//			addSubStringToReportIndicator(name);
//			addStringToReportIndicator("' redefined here");
			compileError(MI, location);
		}
		
		currentMemberBinding = currentMemberBinding->next;
	}
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

// TODO: remove this?
ContextBinding *expectTypeExists(ModuleInformation *MI, ASTnode *node) {
	if (node->nodeType != ASTnodeType_constrainedType) abort();
	linkedList_Node *returnTypeList = NULL;
	buildLLVM(MI, NULL, NULL, NULL, NULL, &returnTypeList, ((ASTnode_constrainedType *)(node->value))->type, 0, 0, 0);
	
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
	
	return ((BuilderType *)returnTypeList->data)->binding;
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

ContextBinding *addFunctionToList(ModuleInformation *MI, linkedList_Node **list, ASTnode *node) {
	ASTnode_function *data = (ASTnode_function *)node->value;
	
	// make sure that the return type actually exists
	ContextBinding* returnType = expectTypeExists(MI, data->returnType);
	
	linkedList_Node *argumentTypes = NULL;
	
	// make sure that the types of all arguments actually exist
	linkedList_Node *currentArgument = data->argumentTypes;
	while (currentArgument != NULL) {
		ContextBinding *currentArgumentBinding = expectTypeExists(MI, (ASTnode *)currentArgument->data);
		
		BuilderType *data = linkedList_addNode(&argumentTypes, sizeof(BuilderType));
		data->binding = currentArgumentBinding;
		
		currentArgument = currentArgument->next;
	}
	
	char *LLVMreturnType = getLLVMtypeFromBinding(MI, returnType);
	
	ContextBinding *functionData = linkedList_addNode(list, sizeof(ContextBinding) + sizeof(ContextBinding_function));
	
	char *functionLLVMname = safeMalloc(data->name->length + 1);
	memcpy(functionLLVMname, data->name->start, data->name->length);
	functionLLVMname[data->name->length] = 0;
	
	functionData->key = data->name;
	functionData->type = ContextBindingType_function;
	// I do not think I need to set byteSize or byteAlign to anything specific
	functionData->byteSize = 0;
	functionData->byteAlign = 0;
	
	((ContextBinding_function *)functionData->value)->LLVMname = functionLLVMname;
	((ContextBinding_function *)functionData->value)->LLVMreturnType = LLVMreturnType;
	((ContextBinding_function *)functionData->value)->argumentNames = data->argumentNames;
	((ContextBinding_function *)functionData->value)->argumentTypes = argumentTypes;
	((ContextBinding_function *)functionData->value)->returnType = getTypeFromBinding(returnType);
	((ContextBinding_function *)functionData->value)->registerCount = 0;
	
	if (compilerOptions.includeDebugInformation) {
		CharAccumulator_appendChars(MI->LLVMmetadataSource, "\n!");
		CharAccumulator_appendInt(MI->LLVMmetadataSource, MI->metadataCount);
		CharAccumulator_appendChars(MI->LLVMmetadataSource, " = distinct !DISubprogram(");
		CharAccumulator_appendChars(MI->LLVMmetadataSource, "name: \"");
		CharAccumulator_appendChars(MI->LLVMmetadataSource, functionLLVMname);
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
		linkedList_Node *currentMemberNode = ((ASTnode_struct *)node->value)->block;
		while (currentMemberNode != NULL) {
			ASTnode *memberNode = (ASTnode *)currentMemberNode->data;
			
			if (memberNode->nodeType == ASTnodeType_variableDefinition) {
				ASTnode_variableDefinition *memberData = (ASTnode_variableDefinition *)memberNode->value;
				
				expectUnusedMemberBindingName(MI, (ContextBinding_struct *)structBinding->value, memberData->name, memberNode->location);
				
				// make sure the type actually exists
				ContextBinding* typeBinding = expectTypeExists(MI, memberData->type);
				
				char *LLVMtype = getLLVMtypeFromBinding(MI, typeBinding);
				
				// if there is a pointer anywhere in the struct then the struct should be aligned by pointer_byteSize
				if (strcmp(LLVMtype, "ptr") == 0) {
					structBinding->byteAlign = pointer_byteSize;
				}
				
				structBinding->byteSize += typeBinding->byteSize;
				
				if (addComma) {
					CharAccumulator_appendChars(outerSource, ", ");
				}
				CharAccumulator_appendChars(outerSource, LLVMtype);
				
				SubString *nameData = linkedList_addNode(&structToGenerate->memberNames, sizeof(SubString));
				memcpy(nameData, memberData->name, sizeof(SubString));
				
				ContextBinding *variableBinding = linkedList_addNode(&((ContextBinding_struct *)structBinding->value)->memberBindings, sizeof(ContextBinding) + sizeof(ContextBinding_variable));
				
				variableBinding->key = memberData->name;
				variableBinding->type = ContextBindingType_variable;
				variableBinding->byteSize = typeBinding->byteSize;
				variableBinding->byteAlign = typeBinding->byteSize;
				
				((ContextBinding_variable *)variableBinding->value)->LLVMRegister = 0;
				((ContextBinding_variable *)variableBinding->value)->LLVMtype = LLVMtype;
				((ContextBinding_variable *)variableBinding->value)->type = getTypeFromBinding(typeBinding);
				
				addComma = 1;
			} else if (memberNode->nodeType == ASTnodeType_function) {
				ASTnode_variableDefinition *memberData = (ASTnode_variableDefinition *)memberNode->value;
				
				expectUnusedMemberBindingName(MI, (ContextBinding_struct *)structBinding->value, memberData->name, memberNode->location);
				
				SubString *nameData = linkedList_addNode(&structToGenerate->memberNames, sizeof(SubString));
				memcpy(nameData, ((ASTnode_function *)memberNode->value)->name, sizeof(SubString));
				
				generateFunction(MI, MI->topLevelFunctionSource, (ContextBinding_function *)addFunctionToList(MI, &structToGenerate->memberBindings, memberNode)->value, memberNode, 1);
			} else {
				abort();
			}
			
			currentMemberNode = currentMemberNode->next;
		}
	}
	
	else {
		linkedList_Node *currentMemberBinding = structToGenerate->memberBindings;
		linkedList_Node *currentMemberName = structToGenerate->memberNames;
		while (currentMemberBinding != NULL) {
			ContextBinding *memberBinding = (ContextBinding *)currentMemberBinding->data;
			
			if (memberBinding->type == ContextBindingType_variable) {
				ContextBinding_variable *variable = (ContextBinding_variable *)memberBinding->value;
				
				if (addComma) {
					CharAccumulator_appendChars(outerSource, ", ");
				}
				CharAccumulator_appendChars(outerSource, variable->LLVMtype);
				
				addComma = 1;
			} else if (memberBinding->type == ContextBindingType_function) {
				ContextBinding_function *function = (ContextBinding_function *)memberBinding->value;
				generateFunction(MI, MI->topLevelFunctionSource, function, NULL, 0);
			} else {
				abort();
			}
			
			currentMemberBinding = currentMemberBinding->next;
			currentMemberName = currentMemberName->next;
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
			
			if (node->nodeType == ASTnodeType_function) {
				// make sure that the name is not already used
				expectUnusedName(MI, ((ASTnode_function *)node->value)->name, node->location);
				
				addFunctionToList(MI, &MI->context.bindings[MI->level], node);
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
				((ContextBinding_struct *)structBinding->value)->memberNames = NULL;
				((ContextBinding_struct *)structBinding->value)->memberBindings = NULL;
				
				generateStruct(MI, outerSource, structBinding, node, 1);
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
				
				ASTnode_import *data = (ASTnode_import *)node->value;
				
				int pathSize = (int)strlen(MI->path) + 1 + data->path->length + 1;
				char *path = safeMalloc(pathSize);
				snprintf(path, pathSize, "%.*s/%s", (int)strlen(MI->path), MI->path, data->path->start);
				
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
				
				break;
			}
				
			case ASTnodeType_struct: {
				if (MI->level != 0) {
					addStringToReportMsg("struct definitions are only allowed at top level");
					compileError(MI, node->location);
				}
				
//				ASTnode_struct *data = (ASTnode_struct *)node->value;
//				
//				ContextBinding *structBinding = getContextBindingFromSubString(MI, data->name);
//				if (structBinding == NULL || structBinding->type != ContextBindingType_struct) abort();
//				ContextBinding_struct *structData = (ContextBinding_struct *)structBinding->value;
				
				break;
			}
			
			case ASTnodeType_function: {
				if (MI->level != 0) {
					printf("function definitions are only allowed at top level\n");
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
					printf("function calls are only allowed in a function\n");
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
					expectSameBinding(MI, ((BuilderType *)expectedTypes->data)->binding, functionToCall->returnType.binding, node->location);
				}
				
				if (types == NULL) {
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
					printf("while loops are only allowed in a function\n");
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
					printf("if statements are only allowed in a function\n");
					compileError(MI, node->location);
				}
				
				ASTnode_if *data = (ASTnode_if *)node->value;
				
				linkedList_Node *expectedTypesForIf = NULL;
				addTypeFromString(MI, &expectedTypesForIf, "Bool");
				
				CharAccumulator expressionSource = {100, 0, 0};
				CharAccumulator_initialize(&expressionSource);
				
				buildLLVM(MI, outerFunction, outerSource, &expressionSource, expectedTypesForIf, NULL, data->expression, 1, 1, 0);
				
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
				
				break;
			}
			
			case ASTnodeType_return: {
				if (outerFunction == NULL) {
					printf("return statements are only allowed in a function\n");
					compileError(MI, node->location);
				}
				
				ASTnode_return *data = (ASTnode_return *)node->value;
				
				if (data->expression == NULL) {
					if (!ifTypeIsNamed(&outerFunction->returnType, "Void")) {
						printf("Returning Void in a function that does not return Void.\n");
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
					printf("variable definitions are only allowed in a function\n");
					compileError(MI, node->location);
				}
				
				ASTnode_variableDefinition *data = (ASTnode_variableDefinition *)node->value;
				
				expectUnusedName(MI, data->name, node->location);
				
				ContextBinding* typeBinding = expectTypeExists(MI, data->type);
				
				char *LLVMtype = getLLVMtypeFromBinding(MI, typeBinding);
				
				CharAccumulator expressionSource = {100, 0, 0};
				CharAccumulator_initialize(&expressionSource);
				
				buildLLVM(MI, outerFunction, outerSource, &expressionSource, typeToList(getTypeFromBinding(typeBinding)), NULL, data->expression, 1, 1, 0);
				
				CharAccumulator_appendChars(outerSource, "\n\t%");
				CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
				CharAccumulator_appendChars(outerSource, " = alloca ");
				CharAccumulator_appendChars(outerSource, LLVMtype);
				CharAccumulator_appendChars(outerSource, ", align ");
				CharAccumulator_appendInt(outerSource, typeBinding->byteAlign);
				
				if (data->expression != NULL) {
					CharAccumulator_appendChars(outerSource, "\n\tstore ");
					CharAccumulator_appendChars(outerSource, expressionSource.data);
					CharAccumulator_appendChars(outerSource, ", ptr %");
					CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
					CharAccumulator_appendChars(outerSource, ", align ");
					CharAccumulator_appendInt(outerSource, typeBinding->byteAlign);
				}
				
				BuilderType type = getTypeFromBinding(typeBinding);
				
				ContextBinding *variableData = linkedList_addNode(&MI->context.bindings[MI->level], sizeof(ContextBinding) + sizeof(ContextBinding_variable));
				
				variableData->key = data->name;
				variableData->type = ContextBindingType_variable;
				variableData->byteSize = pointer_byteSize;
				variableData->byteAlign = pointer_byteSize;
				
				((ContextBinding_variable *)variableData->value)->LLVMRegister = outerFunction->registerCount;
				((ContextBinding_variable *)variableData->value)->LLVMtype = LLVMtype;
				((ContextBinding_variable *)variableData->value)->type = type;
				
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
						printf("variable assignments are only allowed in a function\n");
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
					ASTnode *leftNode = (ASTnode *)data->left->data;
					ASTnode *rightNode = (ASTnode *)data->right->data;
					
					// make sure that both sides of the member access are an identifier
					if (leftNode->nodeType != ASTnodeType_identifier) abort();
					if (rightNode->nodeType != ASTnodeType_identifier) {
						addStringToReportMsg("right side of memberAccess must be an identifier");
						
						addStringToReportIndicator("right side of memberAccess is not an identifier");
						compileError(MI, rightNode->location);
					};
					
					linkedList_Node *leftTypes = NULL;
					buildLLVM(MI, outerFunction, outerSource, &leftInnerSource, NULL, &leftTypes, data->left, 0, 0, 0);
					
					ContextBinding *structBinding = ((BuilderType *)leftTypes->data)->binding;
					if (structBinding->type != ContextBindingType_struct) abort();
					ContextBinding_struct *structData = (ContextBinding_struct *)structBinding->value;
					
					ASTnode_identifier *rightData = (ASTnode_identifier *)rightNode->value;
					
					int index = 0;
					
					linkedList_Node *currentMemberName = structData->memberNames;
					linkedList_Node *currentMemberType = structData->memberBindings;
					while (1) {
						if (currentMemberName == NULL) {
							printf("left side of memberAccess must exist\n");
							compileError(MI, ((ASTnode *)data->left->data)->location);
						}
						
						SubString *memberName = (SubString *)currentMemberName->data;
						ContextBinding *memberBinding = (ContextBinding *)currentMemberType->data;
						
						if (SubString_SubString_cmp(memberName, rightData->name) == 0) {
							if (memberBinding->type == ContextBindingType_variable) {
								if (types != NULL) {
									addTypeFromBuilderType(MI, types, &((ContextBinding_variable *)memberBinding->value)->type);
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
									CharAccumulator_appendChars(outerSource, ((ContextBinding_variable *)memberBinding->value)->LLVMtype);
									CharAccumulator_appendChars(outerSource, ", ptr %");
									CharAccumulator_appendInt(outerSource, outerFunction->registerCount);
									CharAccumulator_appendChars(outerSource, ", align ");
									CharAccumulator_appendInt(outerSource, memberBinding->byteAlign);
									
									if (withTypes) {
										CharAccumulator_appendChars(innerSource, ((ContextBinding_variable *)memberBinding->value)->LLVMtype);
										CharAccumulator_appendChars(innerSource, " ");
									}
									
									outerFunction->registerCount++;
								}
								
								CharAccumulator_appendChars(innerSource, "%");
								CharAccumulator_appendInt(innerSource, outerFunction->registerCount);
								
								outerFunction->registerCount++;
							} else if (memberBinding->type == ContextBindingType_function) {
								if (types != NULL) {
									addTypeFromBinding(MI, types, memberBinding);
								}
//								ContextBinding_function *memberFunction = (ContextBinding_function *)memberBinding->value;
//
//								CharAccumulator_appendChars(innerSource, "@");
//								CharAccumulator_appendChars(innerSource, memberFunction->LLVMname);
							} else {
								abort();
							}
							
							break;
						}
						
						currentMemberName = currentMemberName->next;
						currentMemberType = currentMemberType->next;
						if (memberBinding->type != ContextBindingType_function) {
							index++;
						}
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
							printf("left side of operator expected a number\n");
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
								printf("right side of operator expected a number\n");
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
					printf("unexpected Bool\n");
					compileError(MI, node->location);
				}
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, "i1 ");
				}
				
				CharAccumulator_appendChars(innerSource, "true");
				
				break;
			}

			case ASTnodeType_false: {
				if (!ifTypeIsNamed((BuilderType *)expectedTypes->data, "Bool")) {
					printf("unexpected Bool\n");
					compileError(MI, node->location);
				}
				
				if (withTypes) {
					CharAccumulator_appendChars(innerSource, "i1 ");
				}
				
				CharAccumulator_appendChars(innerSource, "false");
				
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
						expectSameBinding(MI, ((BuilderType *)expectedTypes->data)->binding, variable->type.binding, node->location);
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

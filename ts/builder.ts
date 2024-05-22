import {
	SourceLocation,
	ASTnode,
	ScopeObject,
	unwrapScopeObject,
	CodeGenText,
	getCGText,
	getTypeName,
	ScopeObjectType,
	cast_ScopeObjectType,
	ScopeObject_alias,
	ScopeObject_argument,
	isScopeObjectType,
} from "./types";
import utilities from "./utilities";
import { Indicator, CompileError } from "./report";
import { builtinScopeLevel, builtinCall, getComplexValueFromString } from "./builtin";
import codeGen from "./codeGen";
import { BuilderContext, BuilderOptions } from "./compiler";

function doCodeGen(context: BuilderContext): boolean {
	return !context.options.compileTime;
}

export function getNextSymbolName(context: BuilderContext) {
	return `${context.nextSymbolName++}:${context.file.path}`;
}

export function getTypeText(type: ScopeObjectType) {
	return `'${getTypeName(type)}'`;
}

export function getNameText(scopeObject: ScopeObject): string | null {
	const object = unwrapScopeObject(scopeObject);
	
	if (object.kind == "bool") {
		return `${object.value}`;
	} else if (object.kind == "number") {
		return `${object.value}`;
	} else if (object.kind == "string") {
		return `"${object.value}"`;
	} else if (object.kind == "complexValue") {
		return null;
	} else if (object.kind == "structInstance") {
		return "TODO: getNameText structInstance";
	} else {
		throw utilities.TODO();
	}
}

function getTypeOf(context: BuilderContext, scopeObject: ScopeObject): ScopeObjectType {
	if (scopeObject.kind == "bool") {
		return cast_ScopeObjectType(getAlias(context, "Bool"));
	} else if (scopeObject.kind == "number") {
		return cast_ScopeObjectType(getAlias(context, "Number"));
	} else {
		throw utilities.TODO();
	}
}

function expectType(
	context: BuilderContext,
	expectedType: ScopeObjectType,
	actualType: ScopeObjectType,
	compileError: CompileError
) {
	if (getTypeName(expectedType) == "builtin:Any") {
		return;
	}
	
	if (getTypeName(expectedType) != getTypeName(actualType)) {
		compileError.msg = compileError.msg
			.replace("$expectedTypeName", getTypeName(expectedType))
			.replace("$actualTypeName", getTypeName(actualType));
		throw compileError;
	}
}

export function getAlias(context: BuilderContext, name: string, getProperties?: boolean): ScopeObject_alias | null {
	// builtin
	for (let i = 0; i < builtinScopeLevel.length; i++) {
		const scopeObject = builtinScopeLevel[i];
		if (scopeObject.name == name) {
			return scopeObject;
		}
	}
	
	// scopeLevels
	for (let level = context.file.scope.currentLevel; level >= 0; level--) {
		for (let i = 0; i < context.file.scope.levels[level].length; i++) {
			const scopeObject = context.file.scope.levels[level][i];
			if (!getProperties && scopeObject.isAfield) continue;
			if (scopeObject.name == name) {
				return scopeObject;
			}
		}
	}
	
	// visible
	if (context.file.scope.function) {
		for (let i = 0; i < context.file.scope.function.visible.length; i++) {
			const scopeObject = context.file.scope.function.visible[i];
			if (scopeObject.name == name) {
				return scopeObject;
			}
		}
	}
	
	return null;
}

function getVisibleAsliases(context: BuilderContext): ScopeObject_alias[] {
	let list: ScopeObject_alias[] = [];
	
	// scopeLevels
	for (let level = context.file.scope.currentLevel; level >= 0; level--) {
		for (let i = 0; i < context.file.scope.levels[level].length; i++) {
			const scopeObject = context.file.scope.levels[level][i];
			if (scopeObject.kind == "alias" && scopeObject.isAfield) continue;
			list.push(scopeObject);
		}
	}
	
	// visible
	if (context.file.scope.function) {
		for (let i = 0; i < context.file.scope.function.visible.length; i++) {
			const scopeObject = context.file.scope.function.visible[i];
			if (scopeObject.kind == "alias" && scopeObject.isAfield) continue;
			list.push(scopeObject);
		}
	}
	
	return list;
}

function addAlias(context: BuilderContext, level: number, alias: ScopeObject_alias) {
	if (alias.kind == "alias") {
		if (!alias.isAfield) {
			const oldAlias = getAlias(context, alias.name);
			if (oldAlias) {
				throw new CompileError(`alias '${alias.name}' already exists`)
					.indicator(oldAlias.originLocation, "alias originally defined here")
					.indicator(alias.originLocation, "alias redefined here");
			}
		}
		context.file.scope.levels[level].push(alias);
	} else {
		utilities.unreachable();
	}
}

export function callFunction(
	context: BuilderContext,
	functionToCall: ScopeObject,
	callArguments: ScopeObject[] | null,
	location: SourceLocation,
	comptime: boolean,
	callDest: CodeGenText,
	innerDest: CodeGenText,
	argumentText: CodeGenText | null,
	checkHere?: boolean,
): ScopeObject {
	if (!callArguments && comptime) utilities.unreachable();
	
	if (functionToCall.kind == "function") {
		if (functionToCall.hadError) return undefined as any;
		
		if (callArguments) {
			if (callArguments.length > functionToCall.functionArguments.length) {
				throw new CompileError(`too many arguments passed to function '${functionToCall.symbolName}'`)
					.indicator(location, "function call here");
			}
			
			if (callArguments.length < functionToCall.functionArguments.length) {
				throw new CompileError(`not enough arguments passed to function '${functionToCall.symbolName}'`)
					.indicator(location, "function call here");
			}
		}
		
		if (functionToCall.returnType && functionToCall.comptimeReturn) {
			comptime = true;
		}
		
		let toBeAnalyzedHere: boolean;
		if (checkHere) {
			toBeAnalyzedHere = true;
		} else {
			if (comptime) {
				toBeAnalyzedHere = true;
			} else {
				if (context.inCheckMode) {
					toBeAnalyzedHere = false;
				} else {
					if (functionToCall.toBeGenerated) {
						toBeAnalyzedHere = true;
						functionToCall.toBeGenerated = false;
					} else {
						toBeAnalyzedHere = false;
					}
				}
			}
		}
		
		let result: ScopeObject | undefined;
		
		if (toBeAnalyzedHere) {
			let argumentNameText = "";
			if (callArguments) {
				let nameTextList = [];
				for (let i = 0; i < callArguments.length; i++) {
					const arg = callArguments[i];
					const text = getNameText(arg);
					if (text) {
						nameTextList.push(text);
					}
				}
				argumentNameText = nameTextList.join(", ");
			}
			
			const oldScope = context.file.scope;
			const oldGeneratingFunction = oldScope.generatingFunction;
			context.file.scope = {
				currentLevel: -1,
				levels: [[]],
				function: functionToCall,
				functionArgumentNameText: argumentNameText,
				generatingFunction: oldGeneratingFunction,
			}
			if (!comptime) {
				context.file.scope.generatingFunction = functionToCall;
			}
			
			for (let index = 0; index < functionToCall.functionArguments.length; index++) {
				const argument = functionToCall.functionArguments[index];
				
				if (argument.kind == "argument") {
					let symbolName = argument.name;
					
					if (callArguments) {
						const callArgument = unwrapScopeObject(callArguments[index]);
						
						if (argument.comptime && callArgument.kind == "complexValue") {
							throw new CompileError(`comptime argument '${argument.name}' is not comptime`)
								.indicator(location, "function call here");
						}
						
						expectType(context, argument.type, getTypeOf(context, callArgument),
							new CompileError(`expected type $expectedTypeName but got type $actualTypeName`)
								.indicator(callArgument.originLocation, "argument here")
								.indicator(argument.originLocation, "argument defined here")
						);
						
						addAlias(context, context.file.scope.currentLevel + 1, {
							kind: "alias",
							originLocation: argument.originLocation,
							isAfield: false,
							name: argument.name,
							value: callArgument,
							symbolName: symbolName,
							type: argument.type,
						});
					} else {
						let value: ScopeObject = {
							kind: "complexValue",
							originLocation: argument.originLocation,
							type: argument.type,
						};
						addAlias(context, context.file.scope.currentLevel + 1, {
							kind: "alias",
							originLocation: argument.originLocation,
							isAfield: false,
							name: argument.name,
							value: value,
							symbolName: symbolName,
							type: argument.type,
						});
					}
				} else {
					utilities.unreachable();
				}
			}
			
			let gotError = false;
			
			const text = getCGText();
			try {
				result = build(context, functionToCall.AST, {
					compileTime: comptime,
					codeGenText: text,
					disableDependencyAccess: context.options.disableDependencyAccess,
				}, {
					location: functionToCall.originLocation,
					msg: `function ${functionToCall.symbolName}`,
				}, true, true, false)[0];
			} catch (error) {
				if (error instanceof CompileError) {
					context.errors.push(error);
					gotError = true;
					functionToCall.toBeGenerated = false;
					functionToCall.toBeChecked = false;
					functionToCall.hadError = true;
				} else {
					throw error;
				}
			}
			
			context.file.scope = oldScope;
			context.file.scope.generatingFunction = oldGeneratingFunction;
			
			if (result) {
				const unwrappedResult = unwrapScopeObject(result);
				
				if (functionToCall.returnType) {
					if (comptime) {
						expectType(context, functionToCall.returnType, getTypeOf(context, unwrappedResult),
							new CompileError(`expected type $expectedTypeName but got type $actualTypeName`)
								.indicator(location, "call here")
								.indicator(functionToCall.originLocation, "function defined here")
						);
					} else {
						result = {
							kind: "complexValue",
							originLocation: location,
							type: functionToCall.returnType,
						};
					}
				}
			} else {
				if (functionToCall.returnType && !gotError) {
					throw new CompileError(`non-void function returned void`)
						.indicator(location, "call here")
						.indicator(functionToCall.originLocation, "function defined here");
				}
				
				if (functionToCall.returnType) {
					result = {
						kind: "complexValue",
						originLocation: location,
						type: functionToCall.returnType,
					};
				} else {
					result = {
						kind: "void",
						originLocation: location,
					}
				}
			}
			
			if (!checkHere) {
				if (functionToCall.forceInline) {
					if (callDest) callDest.push(text.join(""));
				} else {
					if (!comptime && !functionToCall.external) {
						codeGen.function(context.topCodeGenText, context, functionToCall, text);
					}
					
					if (innerDest) innerDest.push(text.join(""));
				}
			}
		} else {
			if (callArguments) {
				for (let index = 0; index < functionToCall.functionArguments.length; index++) {
					const argument = functionToCall.functionArguments[index];
					
					if (argument.kind == "argument") {
					
						const callArgument = unwrapScopeObject(callArguments[index]);
						
						if (argument.comptime && callArgument.kind == "complexValue") {
							throw new CompileError(`comptime argument '${argument.name}' is not comptime`)
								.indicator(location, "function call here");
						}
						
						expectType(context, argument.type, getTypeOf(context, callArgument),
							new CompileError(`expected type $expectedTypeName but got type $actualTypeName`)
								.indicator(callArgument.originLocation, "argument here")
								.indicator(argument.originLocation, "argument defined here")
						);
					} else {
						utilities.unreachable();
					}
				}
			}
			
			if (functionToCall.returnType) {
				result = {
					kind: "complexValue",
					originLocation: location,
					type: functionToCall.returnType,
				};
			} else {
				result = {
					kind: "void",
					originLocation: location,
				}
			}
		}
		
		if (!functionToCall.forceInline && !comptime) {
			if (callDest) {
				codeGen.startExpression(callDest, context);
				codeGen.call(callDest, context, functionToCall, argumentText);
				codeGen.endExpression(callDest, context);
			}
		}
		
		// if (!result) throw utilities.unreachable();
		return result as ScopeObject;
	} else {
		utilities.unreachable();
		return {} as ScopeObject;
	}
}

export function build(context: BuilderContext, AST: ASTnode[], options: BuilderOptions | null, sackMarker: Indicator | null, resultAtRet: boolean, addIndentation: boolean, getLevel: boolean): ScopeObject[] {
	context.file.scope.currentLevel++;
	
	let scopeList: ScopeObject[] = [];
	
	if (context.file.scope.levels[context.file.scope.currentLevel] == undefined) {
		context.file.scope.levels[context.file.scope.currentLevel] = [];	
	}
	
	const oldInIndentation = context.inIndentation;
	if (addIndentation && context.file.scope.generatingFunction) {
		context.inIndentation = true;
		context.file.scope.generatingFunction.indentation++;
	} else {
		context.inIndentation = false;
	}
	
	try {
		if (options) {
			const oldOptions = context.options;
			context.options = options;
			scopeList = _build(context, AST, resultAtRet, getLevel);
			context.options = oldOptions;
		} else {
			scopeList = _build(context, AST, resultAtRet, getLevel);
		}
	} catch (error) {
		if (error instanceof CompileError && sackMarker != null) {
			// stdout.write(`stack trace ${getIndicator(sackMarker, true)}`);
		}
		context.file.scope.levels[context.file.scope.currentLevel] = [];
		context.file.scope.currentLevel--;
		throw error;
	}
	
	context.inIndentation = oldInIndentation;
	if (addIndentation && context.file.scope.generatingFunction) {
		context.file.scope.generatingFunction.indentation--;
	}
	
	let level: ScopeObject_alias[] = [];
	if (getLevel == true) {
		level = context.file.scope.levels[context.file.scope.currentLevel];
	}
	
	context.file.scope.levels[context.file.scope.currentLevel] = [];
	context.file.scope.currentLevel--;
	
	if (getLevel == true) {
		return level;
	}
	
	return scopeList;
}

export function _build(context: BuilderContext, AST: ASTnode[], resultAtRet: boolean, getLevel: boolean): ScopeObject[] {
	let scopeList: ScopeObject[] = [];
	
	function addToScopeList(scopeObject: ScopeObject) {
		if (!resultAtRet) {
			scopeList.push(scopeObject);	
		}
	}
	
	for (let i = 0; i < AST.length; i++) {
		const node = AST[i];
		
		if (node.kind == "definition") {
			let value: ScopeObject | null = null;
			if (node.value && node.value.kind == "struct" && !context.options.disableDependencyAccess) {
				value = unwrapScopeObject(build(context, [node.value], {
					codeGenText: null,
					compileTime: context.options.compileTime,
					disableDependencyAccess: true,
				}, null, false, false, false)[0]);
			}
			addAlias(context, context.file.scope.currentLevel, {
				kind: "alias",
				originLocation: node.location,
				isAfield: false,
				name: node.name,
				value: value,
				symbolName: node.name,
				type: null,
			});
		}
	}
	
	for (let index = 0; index < AST.length; index++) {
		const node = AST[index];
		
		if (node.kind == "definition") {
			const alias = getAlias(context, node.name, true);
			if (alias) {
				const valueText = getCGText();
				const value = unwrapScopeObject(build(context, [node.value], {
					codeGenText: valueText,
					compileTime: context.options.compileTime,
					disableDependencyAccess: context.options.disableDependencyAccess,
				}, null, false, false, false)[0]);
				
				if (!value) {
					throw new CompileError(`no value for definition`).indicator(node.location, "here");
				}
				
				// if (value.originLocation != "builtin") {
				// 	const path = value.originLocation.path;
				// 	const line = value.originLocation.line;
				// 	const startColumn = value.originLocation.startColumn;
					
				// 	let symbolName: string;
				// 	if (context.file.scope.function) {
				// 		const functionSymbolName = context.file.scope.function.symbolName;
				// 		const functionArgumentNameText = context.file.scope.functionArgumentNameText;
				// 		symbolName = `${functionSymbolName}(${functionArgumentNameText}).${path}:${line},${startColumn}:${alias.name}`;
				// 	} else {
				// 		symbolName = `${path}:${line},${startColumn}:${alias.name}`;
				// 	}
				// 	if (node.value.kind == "function" && value.kind == "function") {
				// 		value.symbolName = symbolName;
				// 	} else if (node.value.kind == "struct" && value.kind == "typeUse" && value.type.kind == "struct") {
				// 		value.type.name = symbolName;
				// 	}
				// }
				
				alias.value = value;
				alias.type = getTypeOf(context, value);
			} else {
				utilities.unreachable();
			}
			continue;
		}
		
		if (getLevel && context.options.disableDependencyAccess) {
			continue;
		}
		
		switch (node.kind) {
			case "bool": {
				addToScopeList({
					kind: "bool",
					originLocation: node.location,
					value: node.value,
				});
				if (doCodeGen(context)) codeGen.bool(context.options.codeGenText, context, node.value);
				break;
			}
			case "number": {
				addToScopeList({
					kind: "number",
					originLocation: node.location,
					value: node.value,
				});
				if (doCodeGen(context)) codeGen.number(context.options.codeGenText, context, node.value);
				break;
			}
			case "string": {
				addToScopeList({
					kind: "string",
					originLocation: node.location,
					value: node.value,
				});
				if (doCodeGen(context)) codeGen.string(context.options.codeGenText, context, node.value);
				break;
			}
			
			case "identifier": {
				const alias = getAlias(context, node.name);
				if (alias) {
					addToScopeList(alias);
				} else {
					throw new CompileError(`alias '${node.name}' does not exist`).indicator(node.location, "here");
				}
				break;
			}
			case "call": {
				const leftText = getCGText();
				const functionToCall_ = build(context, node.left, {
					compileTime: context.options.compileTime,
					codeGenText: leftText,
					disableDependencyAccess: context.options.disableDependencyAccess,
				}, null, false, false, false)
				if (!functionToCall_[0]) {
					utilities.TODO();
				}
				const functionToCall = unwrapScopeObject(functionToCall_[0]);
				const argumentText = getCGText();
				if (functionToCall_.length > 1) {
					argumentText.push(leftText.join(""));
				}
				const callArguments = build(context, node.callArguments, {
					compileTime: context.options.compileTime,
					codeGenText: argumentText,
					disableDependencyAccess: context.options.disableDependencyAccess,
				}, null, false, false, false);
				
				if (functionToCall.kind == "function") {
					for (let i = 1; i < functionToCall_.length; i++) {
						callArguments.unshift(functionToCall_[i]);
					}
					const result = callFunction(context, functionToCall, callArguments, node.location, context.options.compileTime, context.options.codeGenText, null, argumentText);
					addToScopeList(result);
				} else {
					throw new CompileError(`attempting a function call on something other than a function`)
						.indicator(node.location, "here");
				}
				break;
			}
			case "builtinCall": {
				const argumentText: string[] = [];
				const callArguments = build(context, node.callArguments, {
					compileTime: context.options.compileTime,
					codeGenText: argumentText,
					disableDependencyAccess: context.options.disableDependencyAccess,
				}, null, false, false, false);
				
				const result = builtinCall(context, node, callArguments, argumentText);
				if (result) {
					addToScopeList(result);
				}
				break;
			}
			case "operator": {
				// if (node.operatorText == ".") {
				// 	const leftText = getCGText();
				// 	const left = unwrapScopeObject(build(context, node.left, {
				// 		compileTime: context.options.compileTime,
				// 		codeGenText: leftText,
				// 		disableDependencyAccess: context.options.disableDependencyAccess,
				// 	}, null, false, false, false)[0]);
					
				// 	if (node.right[0].kind != "identifier") {
				// 		throw utilities.TODO();
				// 	}
					
				// 	let addedAlias = false;
					
				// 	if (left.kind == "typeUse") {
				// 		const typeUse = left;
				// 		if (typeUse.type.kind != "struct") {
				// 			throw utilities.unreachable();
				// 		}
						
				// 		for (let i = 0; i < typeUse.type.members.length; i++) {
				// 			const alias = typeUse.type.members[i];
				// 			if (alias.kind == "alias") {
				// 				if (alias.name == node.right[0].name) {
				// 					if (!alias.isAfield && alias.value) {
				// 						addToScopeList(alias);
				// 						addedAlias = true;
				// 						break;
				// 					}
				// 				}
				// 			} else {
				// 				utilities.unreachable();
				// 			}
				// 		}
				// 	}
				// 	else if (left.kind == "structInstance" || left.kind == "complexValue") {
				// 		const typeUse = getTypeOf(context, left);
				// 		if (typeUse.type.kind != "struct") {
				// 			throw utilities.unreachable();
				// 		}
						
				// 		if (left.kind == "structInstance") {
				// 			for (let i = 0; i < left.fields.length; i++) {
				// 				const alias = left.fields[i];
				// 				if (alias.name == node.right[0].name) {
				// 					// if (left.kind == "complexValue") {
				// 					// 	if (alias.isAfield && alias.type) {
				// 					// 		addToScopeList({
				// 					// 			kind: "complexValue",
				// 					// 			originLocation: alias.originLocation,
				// 					// 			type: alias.type,
				// 					// 		});
				// 					// 		addedAlias = true;
				// 					// 		break;
				// 					// 	}
				// 					// } else {
										
				// 					// }
				// 					if (!alias.isAfield && alias.value) {
				// 						addToScopeList(alias);
				// 						addedAlias = true;
				// 						break;
				// 					}
				// 				}
				// 			}
				// 		} else if (left.kind == "complexValue") {
				// 			for (let i = 0; i < typeUse.type.members.length; i++) {
				// 				const alias = typeUse.type.members[i];
				// 				if (alias.name == node.right[0].name) {
				// 					if (alias.isAfield && alias.type) {
				// 						addToScopeList({
				// 							kind: "complexValue",
				// 							originLocation: alias.originLocation,
				// 							type: alias.type,
				// 						});
				// 						addedAlias = true;
				// 						break;
				// 					}
				// 				}
				// 			}
				// 		}
						
				// 		if (!addedAlias) {
				// 			for (let i = 0; i < typeUse.type.members.length; i++) {
				// 				const alias = typeUse.type.members[i];
				// 				if (alias.kind == "alias") {
				// 					if (alias.isAfield) continue;
				// 					if (alias.value && alias.value.kind == "function" && alias.name == node.right[0].name) {
				// 						addToScopeList(alias);
				// 						addToScopeList(left);
				// 						addedAlias = true;
				// 						if (doCodeGen(context) && context.options.codeGenText) {
				// 							context.options.codeGenText.push(leftText.join(""));
				// 						}
				// 						break;
				// 					}
				// 				} else {
				// 					utilities.unreachable();
				// 				}
				// 			}
				// 		} else {
				// 			if (doCodeGen(context)) {
				// 				codeGen.memberAccess(context.options.codeGenText, context, leftText.join(""), node.right[0].name);
				// 			}
				// 		}
				// 	} else {
				// 		utilities.TODO();
				// 	}
					
				// 	if (!addedAlias) {
				// 		throw new CompileError(`no member named '${node.right[0].name}'`)
				// 			.indicator(node.right[0].location, "here");
				// 	}
				// }
				
				{
					const leftText = getCGText();
					const left = unwrapScopeObject(build(context, node.left, {
						compileTime: false,
						codeGenText: leftText,
						disableDependencyAccess: context.options.disableDependencyAccess,
					}, null, false, false, false)[0]);
					const rightText = getCGText();
					const _right = build(context, node.right, {
						compileTime: false,
						codeGenText: rightText,
						disableDependencyAccess: context.options.disableDependencyAccess,
					}, null, false, false, false)[0];
					let right: ScopeObject;
					if (_right.kind == "alias" && _right.isAfield && _right.type) {
						right = {
							kind: "complexValue",
							originLocation: _right.originLocation,
							type: _right.type
						}
					} else {
						right = unwrapScopeObject(_right);
					}
					
					if (left.kind == "complexValue" || right.kind == "complexValue") {
						if (node.operatorText == "+") {
							addToScopeList(getComplexValueFromString(context, "Number"));
						} else if (node.operatorText == "-") {
							addToScopeList(getComplexValueFromString(context, "Number"));
						} else if (node.operatorText == "==") {
							addToScopeList(getComplexValueFromString(context, "Bool"));
						} else if (node.operatorText == "<") {
							addToScopeList(getComplexValueFromString(context, "Bool"));
						} else if (node.operatorText == ">") {
							addToScopeList(getComplexValueFromString(context, "Bool"));
						} else if (node.operatorText == "&&") {
							addToScopeList(getComplexValueFromString(context, "Bool"));
						} else if (node.operatorText == "||") {
							addToScopeList(getComplexValueFromString(context, "Bool"));
						} else {
							utilities.unreachable();
						}
						if (doCodeGen(context)) codeGen.operator(context.options.codeGenText, context, node.operatorText, leftText, rightText);
					}
					
					else if (node.operatorText == "==") {
						if (left.kind == "number" && right.kind == "number") {
							addToScopeList({
								kind: "bool",
								originLocation: node.location,
								value: left.value == right.value,
							});
						} else if (left.kind == "string" && right.kind == "string") {
							addToScopeList({
								kind: "bool",
								originLocation: node.location,
								value: left.value == right.value,
							});
						} else {
							utilities.TODO();
						}
					} else if (node.operatorText == "!=") {
						if (left.kind == "number" && right.kind == "number") {
							addToScopeList({
								kind: "bool",
								originLocation: node.location,
								value: left.value != right.value,
							});
						} else if (left.kind == "string" && right.kind == "string") {
							addToScopeList({
								kind: "bool",
								originLocation: node.location,
								value: left.value != right.value,
							});
						} else {
							utilities.TODO();
						}
					}
					
					else if (left.kind == "bool" && right.kind == "bool") {
						if (node.operatorText == "&&") {
							addToScopeList({
								kind: "bool",
								originLocation: node.location,
								value: left.value && right.value,
							});
						} else if (node.operatorText == "||") {
							addToScopeList({
								kind: "bool",
								originLocation: node.location,
								value: left.value || right.value,
							});
						} else {
							utilities.unreachable();
						}
					}
					
					else if (left.kind == "number" && right.kind == "number") {
						if (node.operatorText == "+") {
							addToScopeList({
								kind: "number",
								originLocation: node.location,
								value: left.value + right.value,
							});
						} else if (node.operatorText == "-") {
							addToScopeList({
								kind: "number",
								originLocation: node.location,
								value: left.value - right.value,
							});
						} else if (node.operatorText == "<") {
							addToScopeList({
								kind: "bool",
								originLocation: node.location,
								value: left.value < right.value,
							});
						} else if (node.operatorText == ">") {
							addToScopeList({
								kind: "bool",
								originLocation: node.location,
								value: left.value > right.value,
							});
						} else {
							utilities.unreachable();
						}
					} else {
						utilities.unreachable();
					}
				}
				
				break;
			}
			
			case "function": {
				let returnType = null;
				if (node.returnType) {
					const _returnType = build(context, [node.returnType.value], null, null, false, false, false)[0];
					if (_returnType.kind == "alias") {
						returnType = cast_ScopeObjectType(_returnType);
					} else {
						utilities.TODO();
					}
				}
				
				let functionArguments: ScopeObject_argument[] = [];
				
				for (let i = 0; i < node.functionArguments.length; i++) {
					const argument = node.functionArguments[i];
					
					if (argument.kind == "argument") {
						const argumentType = cast_ScopeObjectType(build(context, [argument.type.value], {
							codeGenText: null,
							compileTime: true,
							disableDependencyAccess: context.options.disableDependencyAccess,
						}, null, false, false, false)[0]);
						
						functionArguments.push({
							kind: "argument",
							originLocation: argument.location,
							comptime: argument.comptime,
							name: argument.name,
							type: argumentType,
						});	
					} else {
						utilities.unreachable();
					}
				}
				
				const visible = getVisibleAsliases(context);
				
				addToScopeList({
					kind: "function",
					forceInline: node.forceInline,
					external: false,
					toBeGenerated: true,
					toBeChecked: true,
					hadError: false,
					indentation: 0,
					originLocation: node.location,
					symbolName: `${getNextSymbolName(context)}`,
					functionArguments: functionArguments,
					returnType: returnType,
					comptimeReturn: node.comptimeReturn,
					AST: node.codeBlock,
					visible: visible,
				});
				break;
			}
			case "struct": {
				const codeBlockText = getCGText();
				const members = build(context, node.codeBlock, {
					codeGenText: codeBlockText,
					compileTime: context.options.compileTime,
					disableDependencyAccess: context.options.disableDependencyAccess,
				}, null, false, true, true) as ScopeObject_alias[];
				
				const struct: ScopeObject = {
					kind: "struct",
					originLocation: node.location,
					name: `${getNextSymbolName(context)}`,
					toBeChecked: true,
					members: members,
				};
				
				// addToScopeList({
				// 	kind: "typeUse",
				// 	originLocation: node.location,
				// 	type: struct,
				// });
				
				if (!context.options.disableDependencyAccess && doCodeGen(context)) {
					codeGen.struct(context.options.codeGenText, context, struct, codeBlockText);
				}
				break;
			}
			case "while": {
				while (true) {
					const condition = build(context, node.condition, null, null, false, false, false)[0];
					
					if (condition.kind == "bool") {
						if (condition.value) {
							build(context, node.codeBlock, null, null, resultAtRet, true, false)[0];
						} else {
							break;
						}
					} else {
						utilities.TODO();
					}
				}
				break;
			}
			case "if": {
				const conditionText = getCGText();
				const _condition = build(context, node.condition, {
					codeGenText: conditionText,
					compileTime: context.options.compileTime,
					disableDependencyAccess: context.options.disableDependencyAccess,
				}, null, false, false, false)[0];
				
				if (context.inCheckMode) {
					build(context, node.trueCodeBlock, {
						codeGenText: null,
						compileTime: context.options.compileTime,
						disableDependencyAccess: context.options.disableDependencyAccess,
					}, null, resultAtRet, true, false);
					if (node.falseCodeBlock) {
						build(context, node.falseCodeBlock, {
							codeGenText: null,
							compileTime: context.options.compileTime,
							disableDependencyAccess: context.options.disableDependencyAccess,
						}, null, resultAtRet, true, false);
					}
					break;
				}
				
				if (!_condition) {
					throw new CompileError("if statement is missing a condition")
						.indicator(node.location, "here");
				}
				const condition = unwrapScopeObject(_condition);
				
				// If the condition is known at compile time
				if (condition.kind == "bool") {
					if (context.file.scope.generatingFunction) {
						context.file.scope.generatingFunction.indentation--;
					}
					if (condition.value) {
						build(context, node.trueCodeBlock, null, null, resultAtRet, true, false);
					} else {
						if (node.falseCodeBlock) {
							build(context, node.falseCodeBlock, null, null, resultAtRet, true, false);
						}
					}
					if (context.file.scope.generatingFunction) {
						context.file.scope.generatingFunction.indentation++;
					}
				}
				
				// If the condition is not known at compile time, build both code blocks
				else if (condition.kind == "complexValue") {
					const trueText = getCGText();
					const falseText = getCGText();
					build(context, node.trueCodeBlock, {
						codeGenText: trueText,
						compileTime: context.options.compileTime,
						disableDependencyAccess: context.options.disableDependencyAccess,
					}, null, resultAtRet, true, false);
					if (node.falseCodeBlock) {
						build(context, node.falseCodeBlock, {
							codeGenText: falseText,
							compileTime: context.options.compileTime,
							disableDependencyAccess: context.options.disableDependencyAccess,
						}, null, resultAtRet, true, false);
					}
					if (doCodeGen(context)) codeGen.if(context.options.codeGenText, context, conditionText, trueText, falseText);
				}
				
				else {
					utilities.TODO();
				}	
				break;
			}
			case "codeBlock": {
				const text = getCGText();
				build(context, node.codeBlock, {
					codeGenText: text,
					compileTime: context.options.compileTime || node.comptime,
					disableDependencyAccess: context.options.disableDependencyAccess,
				}, null, resultAtRet, true, false);
				break;
			}
			
			case "structInstance": {
				const templateType = cast_ScopeObjectType(build(context, [node.templateStruct.value], {
					codeGenText: [],
					compileTime: context.options.compileTime,
					disableDependencyAccess: context.options.disableDependencyAccess,
				}, null, false, false, false)[0]);
				
				if (templateType.value.kind != "struct") {
					throw utilities.TODO();
				}
				
				const templateStruct = templateType.value;
				
				const fieldText = getCGText();
				const fields = build(context, node.codeBlock, {
					codeGenText: fieldText,
					compileTime: context.options.compileTime,
					disableDependencyAccess: context.options.disableDependencyAccess,
				}, null, false, true, true) as ScopeObject_alias[];
				
				let fieldNames: string[] = [];
				
				// loop over all the fields and make sure that they are supposed to exist
				for (let a = 0; a < fields.length; a++) {
					const field = fields[a];
					fieldNames.push(field.name);
					let fieldShouldExist = false;
					for (let e = 0; e < templateStruct.members.length; e++) {
						const member = templateStruct.members[e];
						if (member.isAfield) {
							if (field.name == member.name) {
								if (!field.type || !member.type) {
									throw utilities.unreachable();
								}
								expectType(context, member.type, field.type,
									new CompileError(`expected type $expectedTypeName but got type $actualTypeName`)
										.indicator(field.originLocation, "field defined here")
										.indicator(member.originLocation, "field originally defined here")
								);
								fieldShouldExist = true;
								break;
							}
						}
					}
					if (!fieldShouldExist) {
						throw new CompileError(`field '${field.name}' should not exist`)
							.indicator(field.originLocation, "field here");
					}
				}
				
				// loop over all of the templates fields, to make sure that there are not any missing fields
				for (let e = 0; e < templateStruct.members.length; e++) {
					const member = templateStruct.members[e];
					if (member.isAfield) {
						if (!fieldNames.includes(member.name)) {
							throw new CompileError(`struct instance is missing field '${member.name}'`)
								.indicator(node.location, "struct instance here")
								.indicator(member.originLocation, "field originally defined here");
						}
					}
				}
				
				const struct: ScopeObject = {
					kind: "structInstance",
					originLocation: node.location,
					templateStruct: templateStruct,
					fields: fields,
				};
				
				addToScopeList(struct);
				
				if (doCodeGen(context)) {
					codeGen.struct(context.options.codeGenText, context, struct, fieldText);
				}
				
				break;
			}
		
			default: {
				utilities.unreachable();
				break;
			}
		}
	}
	
	return scopeList;
}
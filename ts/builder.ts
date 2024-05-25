import utilities from "./utilities";
import codeGen from "./codeGen";
import { Indicator, CompileError } from "./report";
import { builtinScopeLevel, builtinCall, getComplexValueFromString } from "./builtin";
import { BuilderContext, BuilderOptions, CompilerStage } from "./compiler";
import {
	SourceLocation,
	ASTnode,
	ScopeObject,
	unwrapScopeObject,
	CodeGenText,
	getCGText,
	ScopeObjectType_getId,
	ScopeObjectType,
	cast_ScopeObjectType,
	ScopeObject_alias,
	ScopeObject_argument,
	ScopeObject_function,
	ScopeObject_enumCase,
	is_ScopeObjectType,
	ScopeObject_enum,
} from "./types";

function doCodeGen(context: BuilderContext): boolean {
	return !context.options.compileTime;
}

export function getNextSymbolName(context: BuilderContext) {
	return `${context.nextSymbolName++}:${context.file.path}`;
}

export function getTypeText(type: ScopeObjectType) {
	return `'${ScopeObjectType_getId(type)}'`;
}

export function getNameText(scopeObject: ScopeObject): string | null {
	const object = unwrapScopeObject(scopeObject);
	
	if (object.kind == "bool") {
		return `${object.value}`;
	} else if (object.kind == "number") {
		return `${object.value}`;
	} else if (object.kind == "string") {
		return `"${object.value}"`;
	} else if (object.kind == "enumCase") {
		return `"${object.name}"`;
	} else if (object.kind == "complexValue") {
		return null;
	} else if (object.kind == "structInstance") {
		return "TODO: getNameText structInstance";
	} else if (object.kind == "struct") {
		return object.id;
	} else {
		throw utilities.TODO();
	}
}

function getTypeOf(context: BuilderContext, scopeObject: ScopeObject): ScopeObjectType {
	if (scopeObject.kind == "bool") {
		return cast_ScopeObjectType(getAlias(context, "Bool"));
	} else if (scopeObject.kind == "number") {
		return cast_ScopeObjectType(getAlias(context, "Number"));
	} else if (scopeObject.kind == "string") {
		return cast_ScopeObjectType(getAlias(context, "String"));
	} else if (scopeObject.kind == "function") {
		return cast_ScopeObjectType(scopeObject);
	} else if (scopeObject.kind == "complexValue") {
		return scopeObject.type;
	} else if (scopeObject.kind == "structInstance") {
		return scopeObject.template;
	} else if (scopeObject.kind == "enumCase") {
		return scopeObject.parent;
	} else if (scopeObject.kind == "struct") {
		return cast_ScopeObjectType(getAlias(context, "Type"));
	} else if (scopeObject.kind == "enum") {
		return cast_ScopeObjectType(getAlias(context, "Type"));
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
	if (ScopeObjectType_getId(expectedType) == "builtin:Any") {
		return;
	}
	
	const unwrappedExpectedType = unwrapScopeObject(expectedType);
	if (unwrappedExpectedType.kind == "enum") {
		for (let i = 0; i < unwrappedExpectedType.enumerators.length; i++) {
			const enumerator = unwrappedExpectedType.enumerators[i];
			if (enumerator.value.types.length == 0) continue;
			if (
				ScopeObjectType_getId(enumerator.value.types[0])
				==
				ScopeObjectType_getId(actualType)
			) {
				return;
			}
		}
	}
	
	if (ScopeObjectType_getId(expectedType) != ScopeObjectType_getId(actualType)) {
		compileError.msg = compileError.msg
			.replace("$expectedTypeName", ScopeObjectType_getId(expectedType))
			.replace("$actualTypeName", ScopeObjectType_getId(actualType));
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
					.indicator(alias.originLocation, "alias redefined here")
					.indicator(oldAlias.originLocation, "alias originally defined here");
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
		if (functionToCall.hadError) return {
			kind: "complexValue",
			originLocation: location,
			type: functionToCall.returnType,
		};
		
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
				// if (context.inCheckMode) {
				// 	toBeAnalyzedHere = false;
				// } else {
					if (functionToCall.toBeGenerated) {
						toBeAnalyzedHere = true;
						functionToCall.toBeGenerated = false;
					} else {
						toBeAnalyzedHere = false;
					}
				// }
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
							symbolName: symbolName,
							value: callArgument,
							valueAST: null,
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
							symbolName: symbolName,
							value: value,
							valueAST: null,
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
				}, {
					location: functionToCall.originLocation,
					msg: `function ${functionToCall.symbolName}`,
				}, false, true, "no")[0];
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
				
				if (is_ScopeObjectType(unwrappedResult)) {
					if (unwrappedResult.kind == "alias") {
						throw utilities.unreachable();
					}
					if (!unwrappedResult.preIdType) {
						unwrappedResult.id = `(${argumentNameText})`;
						unwrappedResult.preIdType = functionToCall;
					}
				}
				
				// if (unwrappedResult.kind == "enum" || unwrappedResult.kind == "struct") {
				// 	unwrappedResult.id = `${functionToCall.symbolName}(${argumentNameText})`;
				// }
				
				expectType(context, functionToCall.returnType, getTypeOf(context, unwrappedResult),
					new CompileError(`expected type $expectedTypeName but got type $actualTypeName`)
						.indicator(location, "call here")
						.indicator(functionToCall.originLocation, "function defined here")
				);
				
				result = unwrappedResult;
			} else {
				if (!gotError) {
					throw new CompileError(`function did not return`)
						.indicator(functionToCall.originLocation, "function defined here");
				}
				
				result = {
					kind: "complexValue",
					originLocation: location,
					type: functionToCall.returnType,
				};
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
			
			result = {
				kind: "complexValue",
				originLocation: location,
				type: functionToCall.returnType,
			};
		}
		
		if (!functionToCall.forceInline && !comptime) {
			if (callDest) {
				codeGen.startExpression(callDest, context);
				codeGen.call(callDest, context, functionToCall, argumentText);
				codeGen.endExpression(callDest, context);
			}
		}
		
		if (!result) throw utilities.unreachable();
		return result;
	} else {
		throw utilities.unreachable();
	}
}

export function build(context: BuilderContext, AST: ASTnode[], options: BuilderOptions | null, sackMarker: Indicator | null, resultAtRet: boolean, addIndentation: boolean, getLevel: "no" | "yes" | "allowShadowing"): ScopeObject[] {
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
	if (getLevel != "no") {
		level = context.file.scope.levels[context.file.scope.currentLevel];
	}
	
	context.file.scope.levels[context.file.scope.currentLevel] = [];
	context.file.scope.currentLevel--;
	
	if (getLevel != "no") {
		return level;
	}
	
	return scopeList;
}

export function _build(context: BuilderContext, AST: ASTnode[], resultAtRet: boolean, getLevel: "no" | "yes" | "allowShadowing"): ScopeObject[] {
	let scopeList: ScopeObject[] = [];
	
	function addToScopeList(scopeObject: ScopeObject) {
		if (!resultAtRet) {
			scopeList.push(scopeObject);	
		}
	}
	
	for (let i = 0; i < AST.length; i++) {
		const node = AST[i];
		
		if (node.kind == "definition") {
			function buildValue() {
				if (node.kind != "definition") {
					throw utilities.unreachable();
				}
				return unwrapScopeObject(build(context, [node.value], {
					codeGenText: null,
					compileTime: context.options.compileTime,
				}, null, false, false, "no")[0]);
			}
			
			if (
				!context.file.scope.function &&
				context.file.scope.currentLevel == 0 &&
				context.compilerStage > CompilerStage.findAliases
			) {
				const alias = getAlias(context, node.name, true);
				if (!alias) {
					throw utilities.unreachable();
				}
				alias.value = buildValue();
			} else {
				const newAlias: ScopeObject_alias = {
					kind: "alias",
					originLocation: node.location,
					isAfield: false,
					name: node.name,
					symbolName: node.name,
					value: null,
					valueAST: node.value,
				}
				
				if (getLevel == "allowShadowing") {
					if (context.compilerStage > CompilerStage.findAliases) {
						newAlias.value = buildValue();
					}
					context.file.scope.levels[context.file.scope.currentLevel].push(newAlias);
				} else {
					addAlias(context, context.file.scope.currentLevel, newAlias);
					if (context.compilerStage > CompilerStage.findAliases) {
						newAlias.value = buildValue();
					}
				}
			}
		}
	}
	
	for (let index = 0; index < AST.length; index++) {
		const node = AST[index];
		
		if (node.kind == "definition") {
			// const alias = getAlias(context, node.name, true);
			// if (alias) {
			// 	const valueText = getCGText();
			// 	const value = unwrapScopeObject(build(context, [node.value], {
			// 		codeGenText: valueText,
			// 		compileTime: context.options.compileTime,
			// 	}, null, false, false, "no")[0]);
				
			// 	if (!value) {
			// 		throw new CompileError(`no value for definition`).indicator(node.location, "here");
			// 	}
				
			// 	alias.value = value;
			// } else {
			// 	utilities.unreachable();
			// }
			continue;
		}
		
		if (context.compilerStage == CompilerStage.findAliases) {
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
				const left_ = build(context, [node.left], {
					compileTime: context.options.compileTime,
					codeGenText: leftText,
				}, null, false, false, "no")
				if (!left_[0]) {
					utilities.unreachable();
				}
				const left = unwrapScopeObject(left_[0]);
				
				if (left.kind == "enumCase") {
					const fields = build(context, node.callArguments, {
						compileTime: context.options.compileTime,
						codeGenText: null,
					}, null, false, false, "no");
					
					// TODO: checking
					
					addToScopeList({
						kind: "enumInstance",
						originLocation: node.location,
						template: left.parent,
						caseName: left.name,
						fields: fields,
					});
				} else if (left.kind == "function") {
					const argumentText = getCGText();
					if (left_.length > 1) {
						argumentText.push(leftText.join(""));
					}
					const callArguments = build(context, node.callArguments, {
						compileTime: context.options.compileTime,
						codeGenText: argumentText,
					}, null, false, false, "no");
					
					if (left.kind == "function") {
						for (let i = 1; i < left_.length; i++) {
							callArguments.unshift(left_[i]);
						}
						const result = callFunction(context, left, callArguments, node.location, context.options.compileTime, context.options.codeGenText, null, argumentText);
						addToScopeList(result);
					} else {
						throw new CompileError(`attempting a function call on something other than a function`)
							.indicator(node.location, "here");
					}
				} else {
					utilities.TODO();
				}
				break;
			}
			case "builtinCall": {
				if (context.compilerStage != CompilerStage.evaluateAll) {
					if (node.name == "compDebug" || node.name == "compAssert") {
						continue;
					}
				}
				
				const argumentText: string[] = [];
				const callArguments = build(context, node.callArguments, {
					compileTime: context.options.compileTime,
					codeGenText: argumentText,
				}, null, false, false, "no");
				
				const result = builtinCall(context, node, callArguments, argumentText);
				if (result) {
					addToScopeList(result);
				}
				break;
			}
			case "operator": {
				if (node.operatorText == ".") {
					const leftText = getCGText();
					const left = unwrapScopeObject(build(context, node.left, {
						compileTime: context.options.compileTime,
						codeGenText: leftText,
					}, null, false, false, "no")[0]);
					
					if (node.right[0].kind != "identifier") {
						throw utilities.TODO();
					}
					
					let addedAlias = false;
					
					if (left.kind == "struct") {
						for (let i = 0; i < left.members.length; i++) {
							const alias = left.members[i];
							if (alias.kind == "alias") {
								if (alias.name == node.right[0].name) {
									if (!alias.isAfield) {
										addToScopeList(alias);
										addedAlias = true;
										break;
									}
								}
							} else {
								utilities.unreachable();
							}
						}
						
						if (!addedAlias) {
							throw new CompileError(`no member named '${node.right[0].name}'`)
								.indicator(node.right[0].location, "here");
						}
					}
					
					else if (left.kind == "enum") {
						for (let i = 0; i < left.enumerators.length; i++) {
							const enumerator = left.enumerators[i];
							if (enumerator.name == node.right[0].name) {
								addToScopeList(enumerator);
								addedAlias = true;
								break;
							}
						}
						
						if (!addedAlias) {
							throw new CompileError(`no member named '${node.right[0].name}'`)
								.indicator(node.right[0].location, "here");
						}
					}
					
					else if (left.kind == "structInstance" || left.kind == "complexValue") {
						let typeUse = unwrapScopeObject(getTypeOf(context, left));
						if (typeUse.kind == "enumCase") {
							if (!typeUse.types[0]) {
								throw utilities.unreachable();
							}
							typeUse = typeUse.types[0];
						}
						if (typeUse.kind != "struct") {
							throw utilities.unreachable();
						}
						
						//
						// get a field
						//
						
						if (left.kind == "structInstance") {
							for (let i = 0; i < left.fields.length; i++) {
								const alias = left.fields[i];
								if (alias.name == node.right[0].name) {
									// if (left.kind == "complexValue") {
									// 	if (alias.isAfield && alias.type) {
									// 		addToScopeList({
									// 			kind: "complexValue",
									// 			originLocation: alias.originLocation,
									// 			type: alias.type,
									// 		});
									// 		addedAlias = true;
									// 		break;
									// 	}
									// } else {
										
									// }
									if (!alias.isAfield && alias.value) {
										addToScopeList(alias);
										addedAlias = true;
										break;
									}
								}
							}
						} else if (left.kind == "complexValue") {
							for (let i = 0; i < typeUse.fields.length; i++) {
								const field = typeUse.fields[i];
								if (field.name == node.right[0].name) {
									addToScopeList({
										kind: "complexValue",
										originLocation: field.originLocation,
										type: field.type,
									});
									addedAlias = true;
									break;
								}
							}
						}
						
						//
						// get a member on the original struct
						//
						
						if (!addedAlias) {
							for (let i = 0; i < typeUse.members.length; i++) {
								const alias = typeUse.members[i];
								if (alias.kind == "alias") {
									if (alias.isAfield) continue;
									if (alias.value && alias.value.kind == "function" && alias.name == node.right[0].name) {
										addToScopeList(alias);
										addToScopeList(left);
										addedAlias = true;
										if (doCodeGen(context) && context.options.codeGenText) {
											context.options.codeGenText.push(leftText.join(""));
										}
										break;
									}
								} else {
									utilities.unreachable();
								}
							}
						} else {
							if (doCodeGen(context)) {
								codeGen.memberAccess(context.options.codeGenText, context, leftText.join(""), node.right[0].name);
							}
						}
						
						if (!addedAlias) {
							throw new CompileError(`no field named '${node.right[0].name}'`)
								.indicator(node.right[0].location, "here");
						}
					}
					
					else {
						utilities.TODO();
					}
				} else {
					const leftText = getCGText();
					const left = unwrapScopeObject(build(context, node.left, {
						compileTime: context.options.compileTime,
						codeGenText: leftText,
					}, null, false, false, "no")[0]);
					const rightText = getCGText();
					const _right = build(context, node.right, {
						compileTime: context.options.compileTime,
						codeGenText: rightText,
					}, null, false, false, "no")[0];
					let right: ScopeObject;
					if (_right.kind == "alias" && _right.isAfield) {
						right = {
							kind: "complexValue",
							originLocation: _right.originLocation,
							type: getTypeOf(context, unwrapScopeObject(_right))
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
				let returnType = cast_ScopeObjectType(build(context, [node.returnType.value], null, null, false, false, "no")[0]);
				
				let functionArguments: ScopeObject_argument[] = [];
				for (let i = 0; i < node.functionArguments.length; i++) {
					const argument = node.functionArguments[i];
					
					if (argument.kind == "argument") {
						const argumentType = cast_ScopeObjectType(build(context, [argument.type.value], {
							codeGenText: null,
							compileTime: true,
						}, null, false, false, "no")[0]);
						
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
				
				const fn: ScopeObject_function = {
					kind: "function",
					id: `${getNextSymbolName(context)}`,
					preIdType: null,
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
				};
				
				addToScopeList(fn);
				
				if (context.compilerStage == CompilerStage.evaluateAll) {
					callFunction(context, fn, null, "builtin", false, null, null, null, true);
				}
				break;
			}
			case "struct": {
				let fields: ScopeObject_argument[] = [];
				for (let i = 0; i < node.fields.length; i++) {
					const argument = node.fields[i];
					
					if (argument.kind == "argument") {
						const argumentType = cast_ScopeObjectType(build(context, [argument.type.value], {
							codeGenText: null,
							compileTime: true,
						}, null, false, false, "no")[0]);
						
						fields.push({
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
				
				const members = build(context, node.codeBlock, {
					codeGenText: null,
					compileTime: context.options.compileTime,
				}, null, false, true, "yes") as ScopeObject_alias[];
				
				const struct: ScopeObject = {
					kind: "struct",
					originLocation: node.location,
					id: `${getNextSymbolName(context)}`,
					preIdType: null,
					toBeChecked: true,
					fields: fields,
					members: members,
				};
				
				addToScopeList(struct);
				break;
			}
			case "enum": {
				let nextNumber = 0;
				const symbolName = `${getNextSymbolName(context)}`;
				
				const newEnum: ScopeObject = {
					kind: "enum",
					originLocation: node.location,
					id: symbolName,
					preIdType: null,
					toBeChecked: true,
					enumerators: [],
				};
				
				for (let i = 0; i < node.codeBlock.length; i++) {
					const enumeratorNode = node.codeBlock[i];
					
					if (enumeratorNode.kind == "identifier" || enumeratorNode.kind == "definition") {
						let name: string;
						let types: ScopeObjectType[] = [];
						if (enumeratorNode.kind == "identifier") {
							name = enumeratorNode.name;
						// } else if (enumeratorNode.kind == "call") {
							// if (enumeratorNode.left.kind != "identifier") {
							// 	throw utilities.TODO();
							// }
							
							// const _types = build(context, enumeratorNode.callArguments, null, null, false, false, "no");
							// for (let i = 0; i < _types.length; i++) {
							// 	const _type = _types[i];
							// 	if (!is_ScopeObjectType(_type)) {
							// 		throw utilities.TODO();
							// 	}
							// 	types.push(_type);
							// }
							
							// name = enumeratorNode.left.name;
						} else if (enumeratorNode.kind == "definition") {
							const definitionType = build(context, [enumeratorNode.value], null, null, false, false, "no")[0];
							if (!is_ScopeObjectType(definitionType)) {
								throw utilities.TODO();
							}
							
							if (definitionType.kind != "alias") {
								if (!definitionType.preIdType) {
									definitionType.id = `.${enumeratorNode.name}`;
									definitionType.preIdType = newEnum;
								}
							}
							
							types.push(definitionType);
							
							name = enumeratorNode.name;
						} else {
							throw utilities.unreachable();
						}
						
						newEnum.enumerators.push({
							kind: "alias",
							originLocation: enumeratorNode.location,
							isAfield: false,
							name: name,
							symbolName: `${symbolName}.${name}`,
							value: {
								kind: "enumCase",
								originLocation: enumeratorNode.location,
								parent: newEnum,
								name: name,
								number: nextNumber,
								types: types,
							},
							valueAST: null,
						});
						nextNumber++;
					} else {
						utilities.TODO();
					}
				}
				
				addToScopeList(newEnum);
				break;
			}
			case "match": {
				const expression = unwrapScopeObject(build(context, [node.expression], null, null, false, false, "no")[0]);
				let matchName = "";
				let expressionType: ScopeObject_enum;
				if (expression.kind == "enumCase") {
					matchName = expression.name;
					expressionType = expression.parent;
				} else if (expression.kind == "complexValue") {
					const _expressionType = unwrapScopeObject(expression.type);
					if (_expressionType.kind != "enum") {
						throw utilities.TODO();
					}
					
					expressionType = _expressionType;
				} else if (expression.kind == "structInstance") {
					if (!expression.caseName || !expression.caseParent) {
						throw utilities.TODO();
					}
					matchName = expression.caseName;
					expressionType = expression.caseParent;
				} else {
					throw new CompileError("unexpected value in match expression")
						.indicator(node.location, "match here")
				}
				
				let expectedResultType: ScopeObjectType | null = null;
				let expectedResultLocation: SourceLocation | null = null;
				
				for (let i = 0; i < node.codeBlock.length; i++) {
					const caseNode = node.codeBlock[i];
					if (caseNode.kind != "matchCase") {
						throw utilities.TODO();
					}
					
					let enumCase: ScopeObject_enumCase | null = null;
					for (let e = 0; e < expressionType.enumerators.length; e++) {
						const enumerator = expressionType.enumerators[e];
						if (enumerator.name == caseNode.name) {
							enumCase = enumerator.value;
						}
					}
					
					if (!enumCase) {
						throw utilities.TODO();
					}
					
					if (enumCase.types.length > 1) {
						throw utilities.TODO();
					}
					
					for (let i = 0; i < caseNode.types.length; i++) {
						const typeNode = caseNode.types[i];
						
						if (typeNode.kind != "identifier") {
							throw utilities.TODO();
						}
						
						if (expression.kind == "enumCase") {
							addAlias(context, context.file.scope.currentLevel + 1, {
								kind: "alias",
								originLocation: typeNode.location,
								isAfield: false,
								name: typeNode.name,
								symbolName: typeNode.name,
								value: {
									kind: "complexValue",
									originLocation: typeNode.location,
									type: enumCase.types[i],
								},
								valueAST: null,
							});
						} else if (expression.kind == "structInstance") {
							addAlias(context, context.file.scope.currentLevel + 1, {
								kind: "alias",
								originLocation: typeNode.location,
								isAfield: false,
								name: typeNode.name,
								symbolName: typeNode.name,
								value: expression,
								valueAST: null,
							});
						} else {
							addAlias(context, context.file.scope.currentLevel + 1, {
								kind: "alias",
								originLocation: typeNode.location,
								isAfield: false,
								name: typeNode.name,
								symbolName: typeNode.name,
								value: {
									kind: "complexValue",
									originLocation: typeNode.location,
									type: enumCase.types[i],
								},
								valueAST: null,
							});
						}
					}
					
					const caseResult = build(context, caseNode.codeBlock, null, null, false, false, "no")[0];
					const caseResultType = getTypeOf(context, caseResult);
					if (expectedResultType && expectedResultLocation) {
						expectType(context, expectedResultType, caseResultType,
							new CompileError(`different match type expected $expectedTypeName but got type $actualTypeName`)
								.indicator(caseNode.location, "different type here")
								.indicator(expectedResultLocation, "other type here")
						);
					} else {
						expectedResultType = caseResultType;
						expectedResultLocation = caseNode.location;
					}
					
					if (expression.kind != "complexValue") {
						if (caseNode.name == matchName) {
							addToScopeList(caseResult);
						}
					}
				}
				
				if (!expectedResultType || !expectedResultLocation) {
					throw new CompileError("empty match statement")
						.indicator(node.location, "here")
				}
				
				if (expression.kind == "complexValue") {
					addToScopeList({
						kind: "complexValue",
						originLocation: node.location,
						type: expectedResultType,
					});
				}
				
				break;
			}
			case "while": {
				utilities.TODO();
				// while (true) {
				// 	const condition = build(context, node.condition, null, null, false, false, "no")[0];
					
				// 	if (condition.kind == "bool") {
				// 		if (condition.value) {
				// 			build(context, node.codeBlock, null, null, resultAtRet, true, "no")[0];
				// 		} else {
				// 			break;
				// 		}
				// 	} else {
				// 		utilities.TODO();
				// 	}
				// }
				break;
			}
			case "if": {
				const conditionText = getCGText();
				const _condition = build(context, node.condition, {
					codeGenText: conditionText,
					compileTime: context.options.compileTime,
				}, null, false, false, "no")[0];
				
				if (context.compilerStage == CompilerStage.evaluateAliasDependencies) {
					build(context, node.trueCodeBlock, {
						codeGenText: null,
						compileTime: context.options.compileTime,
					}, null, resultAtRet, true, "no");
					if (node.falseCodeBlock) {
						build(context, node.falseCodeBlock, {
							codeGenText: null,
							compileTime: context.options.compileTime,
						}, null, resultAtRet, true, "no");
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
						build(context, node.trueCodeBlock, null, null, resultAtRet, true, "no");
					} else {
						if (node.falseCodeBlock) {
							build(context, node.falseCodeBlock, null, null, resultAtRet, true, "no");
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
					}, null, resultAtRet, true, "no");
					if (node.falseCodeBlock) {
						build(context, node.falseCodeBlock, {
							codeGenText: falseText,
							compileTime: context.options.compileTime,
						}, null, resultAtRet, true, "no");
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
				}, null, resultAtRet, true, "no");
				break;
			}
			
			case "newInstance": {
				let caseName: string | null = null;
				let caseParent: ScopeObject_enum | null = null;
				let _templateType = unwrapScopeObject(build(context, [node.template.value], {
					codeGenText: [],
					compileTime: context.options.compileTime,
				}, null, false, false, "no")[0]);
				if (_templateType.kind == "enumCase") {
					if (_templateType.types.length == 0) {
						utilities.unreachable();
					}
					caseName = _templateType.name;
					caseParent = _templateType.parent;
					_templateType = _templateType.types[0];
				}
				const templateType = cast_ScopeObjectType(_templateType);
				
				if (templateType.kind != "struct") {
					throw utilities.TODO();
				}
				
				const fieldText = getCGText();
				const fieldValues = build(context, node.codeBlock, {
					codeGenText: fieldText,
					compileTime: context.options.compileTime,
				}, null, false, true, "allowShadowing") as ScopeObject_alias[];
				
				let fieldNames: string[] = [];
				
				// loop over all the fields and make sure that they are supposed to exist
				for (let a = 0; a < fieldValues.length; a++) {
					const fieldValue = fieldValues[a];
					fieldNames.push(fieldValue.name);
					let fieldShouldExist = false;
					for (let e = 0; e < templateType.fields.length; e++) {
						const field = templateType.fields[e];
						if (fieldValue.value && fieldValue.name == field.name) {
							expectType(context, field.type, getTypeOf(context, unwrapScopeObject(fieldValue.value)),
								new CompileError(`expected type $expectedTypeName but got type $actualTypeName`)
									.indicator(fieldValue.originLocation, "field defined here")
									.indicator(field.originLocation, "field originally defined here")
							);
							fieldShouldExist = true;
							break;
						}
					}
					if (!fieldShouldExist) {
						throw new CompileError(`field '${fieldValue.name}' should not exist`)
							.indicator(fieldValue.originLocation, "field here");
					}
				}
				
				// loop over all of the templates fields, to make sure that there are not any missing fields
				for (let e = 0; e < templateType.members.length; e++) {
					const member = templateType.members[e];
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
					caseName: caseName,
					caseParent: caseParent,
					template: templateType,
					fields: fieldValues,
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
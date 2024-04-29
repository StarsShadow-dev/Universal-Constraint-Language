import { stdout } from "process";

import {
	SourceLocation,
	ASTnode,
	ScopeObject,
	unwrapScopeObject,
	CodeGenText,
	getCGText,
	getTypeName,
} from "./types";
import utilities from "./utilities";
import { Indicator, getIndicator, CompileError } from "./report";
import { builtinScopeLevel, builtinCall, onCodeGen, getString } from "./builtin";
import codeGen from "./codeGen";

function doCodeGen(context: BuilderContext): boolean {
	return !context.options.compileTime;
}

let nextSymbolName = 0;
export function getNextSymbolName() {
	return nextSymbolName++;
}

export type ScopeInformation = {
	levels: ScopeObject[][],
	currentLevel: number,
	
	function: ScopeObject & { kind: "function" } | null,
	
	// the function that is being generated
	generatingFunction: ScopeObject & { kind: "function" } | null,
}

export type BuilderOptions = {
	compileTime: boolean,
	codeGenText: CodeGenText,
	disableValueEvaluation: boolean,
}

export type BuilderContext = {
	topCodeGenText: string[],
	filePath: string,
	scope: ScopeInformation,
	options: BuilderOptions,
}

function getAsComptimeType(type: ScopeObject, location?: SourceLocation): ScopeObject & { kind: "typeUse" } {
	if (type.kind == "typeUse") {
		if (!location) location = "builtin";
		return {
			kind: "typeUse",
			originLocation: location,
			comptime: true,
			type: type.type,
		}	
	} else {
		throw utilities.unreachable();
	}
}

export function getTypeDescription(type: ScopeObject & { kind: "typeUse" }) {
	return `${getTypeName(type)}`;
}

export function getTypeOf(context: BuilderContext, value: ScopeObject): ScopeObject & { kind: "typeUse" } {
	if (value.kind == "bool") {
		return getAsComptimeType(unwrapScopeObject(getAlias(context, "Bool")));
	} else if (value.kind == "number") {
		return getAsComptimeType(unwrapScopeObject(getAlias(context, "Number")));
	} else if (value.kind == "string") {
		return getAsComptimeType(unwrapScopeObject(getAlias(context, "String")));
	} else if (value.kind == "typeUse") {
		if (value.type.kind == "struct") {
			if (value.type.templateStruct) {
				return getTypeOf(context, value.type.templateStruct);
			} else {
				return value;
			}
		}
		return getAsComptimeType(unwrapScopeObject(getAlias(context, "Type")));
	} else if (value.kind == "complexValue") {
		return value.type;
	} else if (value.kind == "function") {
		return {
			kind: "typeUse",
			originLocation: value.originLocation,
			comptime: false,
			type: value,
		};
	} else if (value.kind == "struct") {
		return {
			kind: "typeUse",
			originLocation: value.originLocation,
			comptime: false,
			type: value,
		};
	} else {
		throw utilities.unreachable();
	}
}

function expectType(context: BuilderContext, expected: ScopeObject, actual: ScopeObject, compileError: CompileError) {
	let expectedType: ScopeObject = expected;
	
	let actualType = getTypeOf(context, actual);
	
	if (expectedType.kind == "typeUse" && actualType.kind == "typeUse") {
		if (getTypeName(expectedType) == "builtin:Any") {
			return;
		}
		
		if (expectedType == unwrapScopeObject(getAlias(context, "Type"))) {
			utilities.TODO();
		}
		
		if (expectedType.comptime && !actualType.comptime) {
			compileError.msg = `expected type ${getTypeDescription(expectedType)} that is a compile time type, but got type ${getTypeDescription(actualType)} that is not a compile time type`;
			throw compileError;
		}
		
		if (getTypeName(expectedType) != getTypeName(actualType)) {
			compileError.msg = compileError.msg
				.replace("$expectedTypeName", getTypeName(expectedType))
				.replace("$actualTypeName", getTypeName(actualType));
			throw compileError;
		}
	} else {
		utilities.unreachable();
	}
}

function getAlias(context: BuilderContext, name: string, getProperties?: boolean): ScopeObject & { kind: "alias" } | null {
	// builtin
	for (let i = 0; i < builtinScopeLevel.length; i++) {
		const scopeObject = builtinScopeLevel[i];
		if (scopeObject.kind == "alias") {
			if (scopeObject.name == name) {
				return scopeObject;
			}
		} else {
			utilities.unreachable();
		}
	}
	
	// scopeLevels
	for (let level = context.scope.currentLevel; level >= 0; level--) {
		for (let i = 0; i < context.scope.levels[level].length; i++) {
			const scopeObject = context.scope.levels[level][i];
			if (scopeObject.kind == "alias") {
				if (!getProperties && scopeObject.isAproperty) continue;
				if (scopeObject.name == name) {
					return scopeObject;
				}
			} else {
				utilities.unreachable();
			}
		}
	}
	
	// visible
	if (context.scope.function) {
		for (let i = 0; i < context.scope.function.visible.length; i++) {
			const scopeObject = context.scope.function.visible[i];
			if (scopeObject.kind == "alias") {
				if (scopeObject.name == name) {
					return scopeObject;
				}
			} else {
				utilities.unreachable();
			}
		}
	}
	
	return null;
}

function getVisibleAsliases(context: BuilderContext): ScopeObject[] {
	let list: ScopeObject[] = [];
	
	// scopeLevels
	for (let level = context.scope.currentLevel; level >= 0; level--) {
		for (let i = 0; i < context.scope.levels[level].length; i++) {
			const scopeObject = context.scope.levels[level][i];
			if (scopeObject.kind == "alias" && scopeObject.isAproperty) continue;
			list.push(scopeObject);
		}
	}
	
	// visible
	if (context.scope.function) {
		for (let i = 0; i < context.scope.function.visible.length; i++) {
			const scopeObject = context.scope.function.visible[i];
			if (scopeObject.kind == "alias" && scopeObject.isAproperty) continue;
			list.push(scopeObject);
		}
	}
	
	return list;
}

function addAlias(context: BuilderContext, level: number, alias: ScopeObject) {
	if (alias.kind == "alias") {
		const oldAlias = getAlias(context, alias.name)
		if (oldAlias) {
			throw new CompileError(`alias '${alias.name}' already exists`)
				.indicator(oldAlias.originLocation, "alias originally defined here")
				.indicator(alias.originLocation, "alias redefined here");
		}
		context.scope.levels[level].push(alias);
	} else {
		utilities.unreachable();
	}
}

export function callFunction(context: BuilderContext, functionToCall: ScopeObject, callArguments: ScopeObject[], location: SourceLocation, fillComplex: boolean, compileTime: boolean, callDest: CodeGenText, innerDest: CodeGenText, argumentText: CodeGenText | null): ScopeObject {
	if (functionToCall.kind == "function") {
		if (!fillComplex) {
			if (callArguments.length > functionToCall.functionArguments.length) {
				throw new CompileError(`too many arguments passed to function '${functionToCall.symbolName}'`)
					.indicator(location, "function call here");
			}
			
			if (callArguments.length < functionToCall.functionArguments.length) {
				throw new CompileError(`not enough arguments passed to function '${functionToCall.symbolName}'`)
					.indicator(location, "function call here");
			}	
		}
		
		if (functionToCall.returnType && getTypeName(functionToCall.returnType) == "builtin:Any") {
			if (!functionToCall.returnType.comptime) {
				throw new CompileError(`a function cannot return ${getTypeDescription(functionToCall.returnType)} that is not comptime`)
					.indicator(functionToCall.originLocation, "function defined here");
			}
		}
		
		let toBeGeneratedHere: boolean;
		if (compileTime) {
			toBeGeneratedHere = true;
		} else {
			if (functionToCall.returnType && functionToCall.returnType.comptime) {
				toBeGeneratedHere = true;
			} else {
				if (functionToCall.toBeGenerated) {
					toBeGeneratedHere = true;
					functionToCall.toBeGenerated = false;
				} else {
					toBeGeneratedHere = false;
				}
			}
		}
		
		let result: ScopeObject;
		
		if (toBeGeneratedHere) {
			const oldScope = context.scope;
			context.scope = {
				currentLevel: -1,
				levels: [[]],
				function: functionToCall,
				generatingFunction: oldScope.generatingFunction,
			}
			
			if (!compileTime) {
				context.scope.generatingFunction = functionToCall;
			}
			
			for (let index = 0; index < functionToCall.functionArguments.length; index++) {
				const argument = functionToCall.functionArguments[index];
				
				const argumentType = unwrapScopeObject(argument.type);
				
				if (argument.kind == "argument" && argumentType.kind == "typeUse") {
					let symbolName = argument.name;
					// {
					// 	const temp = callArguments[index];
					// 	if (temp.kind == "alias") {
					// 		symbolName = temp.symbolName;
					// 	}
					// }
					
					if (fillComplex) {
						addAlias(context, context.scope.currentLevel + 1, {
							kind: "alias",
							originLocation: argument.originLocation,
							mutable: false,
							isAproperty: false,
							name: argument.name,
							value: {
								kind: "complexValue",
								originLocation: argument.originLocation,
								type: argumentType,
							},
							symbolName: symbolName,
							type: argumentType,
						});
					} else {
						const callArgument = unwrapScopeObject(callArguments[index]);
						
						expectType(context, argumentType, callArgument,
							new CompileError(`expected type $expectedTypeName but got type $actualTypeName`)
								.indicator(callArgument.originLocation, "argument here")
								.indicator(argument.originLocation, "argument defined here")
						);
						
						addAlias(context, context.scope.currentLevel + 1, {
							kind: "alias",
							originLocation: argument.originLocation,
							mutable: false,
							isAproperty: false,
							name: argument.name,
							value: callArgument,
							symbolName: symbolName,
							type: argumentType,
						});	
					}
				} else {
					utilities.unreachable();
				}
			}
			
			const text = getCGText();
			
			result = build(context, functionToCall.AST, {
				compileTime: compileTime,
				codeGenText: text,
				disableValueEvaluation: context.options.disableValueEvaluation,
			}, {
				location: functionToCall.originLocation,
				msg: `function ${functionToCall.symbolName}`,
			}, true)[0];
			
			context.scope = oldScope;
			
			if (result) {
				const unwrappedResult = unwrapScopeObject(result);
				
				if (result.originLocation != "builtin") {
					if (
						unwrappedResult.kind == "typeUse" && unwrappedResult.type.kind == "struct" &&
						!unwrappedResult.type.templateStruct
					) {
						let nameArgumentsText = "";
						for (let i = 0; i < callArguments.length; i++) {
							const arg = callArguments[i];
							if (arg.kind == "string") {
								nameArgumentsText += `"${arg.value}"`;
							}
						}
						unwrappedResult.type.name = `${functionToCall.symbolName}(${nameArgumentsText})`;
					}
				}
				
				if (functionToCall.returnType) {
					expectType(context, unwrapScopeObject(functionToCall.returnType), unwrappedResult,
						new CompileError(`expected type $expectedTypeName but got type $actualTypeName`)
							.indicator(location, "call here")
							.indicator(functionToCall.originLocation, "function defined here")
					);
				} else {
					throw new CompileError(`void function returned a value`)
						.indicator(location, "function called here")
						.indicator(functionToCall.originLocation, "function defined here");
				}
			} else {
				if (functionToCall.returnType) {
					throw new CompileError(`non-void function returned void`)
						.indicator(location, "call here")
						.indicator(functionToCall.originLocation, "function defined here");
				}
				result = {
					kind: "void",
					originLocation: location,
				}
			}
			
			if (functionToCall.forceInline) {
				if (callDest) callDest.push(...text);
			} else {
				if (!compileTime) {
					codeGen.function(context.topCodeGenText, context, functionToCall, text);
				}
				
				if (innerDest) innerDest.push(...text);
			}
		} else {
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
		
		if (!functionToCall.forceInline && !compileTime) {
			if (callDest) codeGen.call(callDest, context, functionToCall, argumentText);
		}
		
		return result;
	} else {
		utilities.unreachable();
		return {} as ScopeObject;
	}
}

export function build(context: BuilderContext, AST: ASTnode[], options: BuilderOptions | null, sackMarker: Indicator | null, resultAtRet: boolean, getLevel?: boolean): ScopeObject[] {
	context.scope.currentLevel++;
	
	let scopeList: ScopeObject[] = [];
	
	if (context.scope.levels[context.scope.currentLevel] == undefined) {
		context.scope.levels[context.scope.currentLevel] = [];	
	}
	
	try {
		if (options) {
			const oldOptions = context.options;
			context.options = options;
			scopeList = _build(context, AST, resultAtRet);
			context.options = oldOptions;
		} else {
			scopeList = _build(context, AST, resultAtRet);
		}
	} catch (error) {
		if (error instanceof CompileError && sackMarker != null) {
			stdout.write(`stack trace ${getIndicator(sackMarker, true)}`);
		}
		context.scope.levels[context.scope.currentLevel] = [];
		context.scope.currentLevel--;
		throw error;
	}
	
	let level: ScopeObject[] = [];
	if (getLevel == true) {
		level = context.scope.levels[context.scope.currentLevel];
	}
	
	if (context.scope.currentLevel != 0) {
		context.scope.levels[context.scope.currentLevel] = [];
	}
	context.scope.currentLevel--;
	
	if (getLevel == true) {
		return level;
	}
	
	return scopeList;
}

export function _build(context: BuilderContext, AST: ASTnode[], resultAtRet: boolean): ScopeObject[] {
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
			if (node.value && node.value.kind == "struct" && !context.options.disableValueEvaluation) {
				value = unwrapScopeObject(build(context, [node.value], {
					codeGenText: null,
					compileTime: context.options.compileTime,
					disableValueEvaluation: true,
				}, null, false)[0]);
			}
			addAlias(context, context.scope.currentLevel, {
				kind: "alias",
				originLocation: node.location,
				mutable: node.mutable,
				isAproperty: node.isAproperty,
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
				const typeText = getCGText();
				let type = undefined as any as ScopeObject;
				
				if (node.type) {
					type = unwrapScopeObject(build(context, [node.type.value], {
						codeGenText: typeText,
						compileTime: context.options.compileTime,
						disableValueEvaluation: context.options.disableValueEvaluation,
					}, null, false)[0]);
				}
				
				if (node.value && !context.options.disableValueEvaluation) {
					const valueText = getCGText();
					const value = unwrapScopeObject(build(context, [node.value], {
						codeGenText: valueText,
						compileTime: context.options.compileTime,
						disableValueEvaluation: context.options.disableValueEvaluation,
					}, null, false)[0]);
					
					if (!value) {
						throw new CompileError(`no value for alias`).indicator(node.location, "here");
					}
					
					if (node.type) {
						expectType(context, type, value,
							new CompileError(`definition expected type $expectedTypeName but got type $actualTypeName`)
								.indicator(node.location, "definition here")
						);
					} else {
						type = getTypeOf(context, value);
					}
					
					if (value.originLocation != "builtin") {
						if (node.value.kind == "function" && value.kind == "function") {
							value.symbolName += `:${value.originLocation.path}:${alias.name}`;
						} else if (node.value.kind == "struct" && value.kind == "typeUse" && value.type.kind == "struct") {
							value.type.name += `:${value.originLocation.path}:${alias.name}`;
						}
					}
					
					if (alias.value) {
						if (
							alias.value.kind == "typeUse" && alias.value.type.kind == "struct" && 
							value.kind == "typeUse" && value.type.kind == "struct"
						) {
							alias.value.type.name = value.type.name;
							alias.value.type.properties = value.type.properties;
						} else {
							utilities.TODO();
						}
					} else {
						alias.value = value;
					}
					
					if (doCodeGen(context)) {
						if (!(type.kind == "typeUse" && type.comptime)) {
							codeGen.alias(context.options.codeGenText, context, alias, valueText);
						}
					}
				}
				
				if (type) {
					if (type.kind == "typeUse") {
						alias.type = type;
					} else {
						throw utilities.unreachable();
					}
				}
			} else {
				utilities.unreachable();
			}
			continue;
		}
		
		switch (node.kind) {
			case "bool": {
				addToScopeList({
					kind: "bool",
					originLocation: node.location,
					value: node.value,
				});
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
					if (alias.value) {
						if (doCodeGen(context)) codeGen.load(context.options.codeGenText, context, alias);
						addToScopeList(alias);
					} else {
						throw new CompileError(`alias '${node.name}' used before its definition`)
							.indicator(node.location, "identifier here")
							.indicator(alias.originLocation, "alias defined here");
					}
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
					disableValueEvaluation: context.options.disableValueEvaluation,
				}, null, false)[0]
				if (!functionToCall_) {
					utilities.TODO();
				}
				const functionToCall = unwrapScopeObject(functionToCall_);
				const argumentText = getCGText();
				const callArguments = build(context, node.callArguments, {
					compileTime: context.options.compileTime,
					codeGenText: argumentText,
					disableValueEvaluation: context.options.disableValueEvaluation,
				}, null, false);
				
				if (functionToCall.kind == "function") {
					const result = callFunction(context, functionToCall, callArguments, node.location, false, context.options.compileTime, context.options.codeGenText, null, argumentText);
					
					addToScopeList(result);
				} else {
					throw new CompileError(`attempting a function call on something other than a function`)
						.indicator(node.location, "here");
				}
				break;
			}
			case "builtinCall": {
				const argumentText = getCGText();
				const callArguments = build(context, node.callArguments, {
					compileTime: context.options.compileTime,
					codeGenText: argumentText,
					disableValueEvaluation: context.options.disableValueEvaluation,
				}, null, false);
				const result = builtinCall(context, node, callArguments);
				if (result) {
					addToScopeList(result);
				}
				break;
			}
			case "operator": {
				if (node.operatorText == "=") {
					const leftText = getCGText();
					const left = build(context, node.left, {
						compileTime: context.options.compileTime,
						codeGenText: leftText,
						disableValueEvaluation: context.options.disableValueEvaluation,
					}, null, false)[0];
					const rightText = getCGText();
					const right = build(context, node.right, {
						compileTime: context.options.compileTime,
						codeGenText: rightText,
						disableValueEvaluation: context.options.disableValueEvaluation,
					}, null, false)[0];
					
					if (left.kind == "alias") {
						if (!left.mutable) {
							throw new CompileError(`the alias '${left.name}' is not mutable`)
								.indicator(node.location, "reassignment here")
								.indicator(left.originLocation, "alias defined here");
						}
						
						if (doCodeGen(context)) {
							if (left.type) {
								if (!(left.type.kind == "typeUse" && left.type.comptime)) {
									codeGen.set(context.options.codeGenText, context, left, leftText, rightText);
								}
							} else {
								utilities.unreachable();
							}
						}
						
						left.value = right;
					} else {
						throw new CompileError(`attempted to assign to something other than an alias`)
							.indicator(node.location, "reassignment here");
					}
				}
				
				else if (node.operatorText == ".") {
					const left = unwrapScopeObject(build(context, node.left, null, null, false)[0]);
					
					let typeUse: ScopeObject;
					if (left.kind == "typeUse") {
						typeUse = left;
					} else if (left.kind == "complexValue") {
						typeUse = left.type;
					} else {
						throw utilities.unreachable();
					}
					
					if (typeUse.kind == "typeUse" && typeUse.type.kind == "struct" && node.right[0].kind == "identifier") {
						let addedAlias = false;
						for (let i = 0; i < typeUse.type.properties.length; i++) {
							const alias = typeUse.type.properties[i];
							if (alias.kind == "alias") {
								if (alias.isAproperty) continue;
								if (alias.value && alias.name == node.right[0].name) {
									addToScopeList(alias);
									addedAlias = true;
								}
							} else {
								utilities.unreachable();
							}
						}
						if (!addedAlias) {
							throw new CompileError(`no member named '${node.right[0].name}'`)
								.indicator(node.right[0].location, "here");
						}
					} else {
						utilities.TODO();
					}
				}
				
				else {
					const left = unwrapScopeObject(build(context, node.left, {
						compileTime: false,
						codeGenText: null,
						disableValueEvaluation: context.options.disableValueEvaluation,
					}, null, false)[0]);
					const right = unwrapScopeObject(build(context, node.right, {
						compileTime: false,
						codeGenText: null,
						disableValueEvaluation: context.options.disableValueEvaluation,
					}, null, false)[0]);
					
					if (node.operatorText == "==") {
						if (left.kind == "number" && right.kind == "number") {
							addToScopeList({
								kind: "bool",
								originLocation: node.location,
								value: left.value == right.value,
							});
						} else {
							utilities.TODO();
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
					}
				}
				
				break;
			}
			case "comptime": {
				const value = unwrapScopeObject(build(context, [node.value], null, null, false)[0]);
				if (value.kind == "typeUse") {
					addToScopeList(getAsComptimeType(value, node.location));
				} else {
					utilities.unreachable();
				}
				break;
			}
			
			case "function": {
				let returnType = null;
				if (node.returnType) {
					const _returnType = unwrapScopeObject(build(context, [node.returnType.value], null, null, false)[0]);
					if (_returnType.kind == "typeUse") {
						returnType = _returnType;
					} else {
						utilities.TODO();
					}
				}
				
				let functionArguments: (ScopeObject & { kind: "argument" })[] = [];
				
				for (let i = 0; i < node.functionArguments.length; i++) {
					const argument = node.functionArguments[i];
					
					if (argument.kind == "argument") {
						const argumentType = unwrapScopeObject(build(context, [argument.type.value], {
							codeGenText: null,
							compileTime: true,
							disableValueEvaluation: context.options.disableValueEvaluation,
						}, null, false)[0]);
						
						if (argumentType.kind == "typeUse") {
							functionArguments.push({
								kind: "argument",
								originLocation: argument.location,
								name: argument.name,
								type: argumentType,
							});	
						} else {
							utilities.TODO();
						}
					} else {
						utilities.unreachable();
					}
				}
				
				const visible = getVisibleAsliases(context);
				
				addToScopeList({
					kind: "function",
					forceInline: node.forceInline,
					originLocation: node.location,
					symbolName: `${getNextSymbolName()}`,
					functionArguments: functionArguments,
					returnType: returnType,
					AST: node.codeBlock,
					visible: visible,
					toBeGenerated: true,
				});
				break;
			}
			case "struct": {
				const properties = build(context, node.codeBlock, null, null, false, true);
				
				let templateStruct: (ScopeObject & { kind: "struct" }) | null = null;
				
				if (node.templateType) {
					const templateType = unwrapScopeObject(build(context, [node.templateType.value], null, null, false)[0]);
					
					if (templateType.kind == "typeUse" && templateType.type.kind == "struct") {
						templateStruct = templateType.type;
						for (let e = 0; e < templateStruct.properties.length; e++) {
							const expectedProperty = templateStruct.properties[e];
							if (expectedProperty.kind == "alias" && expectedProperty.isAproperty) {
								let foundActualProperty = false;
								for (let a = 0; a < properties.length; a++) {
									const actualProperty = properties[a];
									if (actualProperty.kind == "alias" && actualProperty.name == expectedProperty.name) {
										foundActualProperty = true;
										if (context.options.disableValueEvaluation) {
											continue;
										}
										if (!expectedProperty.type) {
											throw utilities.unreachable();
										}
										expectType(context, expectedProperty.type, unwrapScopeObject(actualProperty),
											new CompileError(`struct property '${expectedProperty.name}' expected type $expectedTypeName but got type $actualTypeName`)
												.indicator(actualProperty.originLocation, "here")
												.indicator(templateStruct.originLocation, "template struct defined here")
												.indicator(expectedProperty.originLocation, "property originally defined here")
										);
									}
								}
								if (!foundActualProperty) {
									throw new CompileError(`property '${expectedProperty.name}' does not exist on struct`)
										.indicator(node.location, "struct defined here")
										.indicator(expectedProperty.originLocation, "missing property defined here");
								}
							}
						}
						for (let a = 0; a < properties.length; a++) {
							const actualProperty = properties[a];
							if (actualProperty.kind == "alias") {
								let propertySupposedToExist = false;
								for (let e = 0; e < templateStruct.properties.length; e++) {
									const expectedProperty = templateStruct.properties[e];
									if (expectedProperty.kind == "alias" && expectedProperty.isAproperty) {
										if (actualProperty.name == expectedProperty.name) {
											propertySupposedToExist = true;
											break;
										}
									}
								}
								if (!propertySupposedToExist) {
									throw new CompileError(`property '${actualProperty.name}' should not exist on struct`)
										.indicator(actualProperty.originLocation, "property defined here")
										.indicator(templateStruct.originLocation, "template struct defined here");
								}
							}
						}
					} else {
						throw new CompileError(`a struct can only use another struct as a template`)
							.indicator(node.location, "struct defined here");
					}
				}
				
				addToScopeList({
					kind: "typeUse",
					originLocation: node.location,
					comptime: false,
					type: {
						kind: "struct",
						originLocation: node.location,
						name: `${getNextSymbolName()}`,
						templateStruct: templateStruct,
						properties: properties,
					},
				});
				break;
			}
			case "codeGenerate": {
				build(context, node.codeBlock, {
					codeGenText: context.options.codeGenText,
					compileTime: false,
					disableValueEvaluation: context.options.disableValueEvaluation,
				}, null, false);
				break;
			}
			case "while": {
				while (true) {
					const condition = build(context, node.condition, null, null, false)[0];
					
					if (condition.kind == "bool") {
						if (condition.value) {
							build(context, node.codeBlock, null, null, resultAtRet)[0];
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
					disableValueEvaluation: context.options.disableValueEvaluation,
				}, null, false)[0];
				if (!_condition) {
					throw new CompileError("if statement is missing a condition")
						.indicator(node.location, "here");
				}
				const condition = unwrapScopeObject(_condition);
				
				// If the condition is known at compile time
				if (condition.kind == "bool") {
					if (condition.value) {
						build(context, node.trueCodeBlock, null, null, resultAtRet);
					} else {
						if (node.falseCodeBlock) {
							build(context, node.falseCodeBlock, null, null, resultAtRet);
						}
					}
				}
				
				// If the condition is not known at compile time, build both code blocks
				else if (condition.kind == "complexValue") {
					const trueText = getCGText();
					const falseText = getCGText();
					build(context, node.trueCodeBlock, {
						codeGenText: trueText,
						compileTime: context.options.compileTime,
						disableValueEvaluation: context.options.disableValueEvaluation,
					}, null, resultAtRet);
					if (node.falseCodeBlock) {
						build(context, node.falseCodeBlock, {
							codeGenText: falseText,
							compileTime: context.options.compileTime,
							disableValueEvaluation: context.options.disableValueEvaluation,
						}, null, resultAtRet);
					}
					if (doCodeGen(context)) codeGen.if(context.options.codeGenText, context, conditionText, trueText, falseText);
				}
				
				else {
					utilities.TODO();
				}	
				break;
			}
			case "return": {
				if (!resultAtRet) {
					throw new CompileError("unexpected return").indicator(node.location, "here");
				}
				if (node.value) {
					const value = build(context, [node.value], null, null, false)[0];
					scopeList.push(value);	
				}
				return scopeList;
			}
		
			default: {
				utilities.unreachable();
				break;
			}
		}
	}
	
	return scopeList;
}
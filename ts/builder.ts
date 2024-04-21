import { stdout } from "process";

import {
	SourceLocation,
	ASTnode,
	ScopeObject,
	unwrapScopeObject,
	CodeGenText,
	getCGText,
} from "./types";
import utilities from "./utilities";
import { Indicator, getIndicator, CompileError } from "./report";
import { builtinScopeLevel, builtinCall, onCodeGen, getString } from "./builtin";
import codeGen from "./codeGen";

function doCodeGen(context: BuilderContext): boolean {
	return !context.options.compileTime;
}

let nextSymbolName = 0;

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
}

export type BuilderContext = {
	filePath: string,
	scope: ScopeInformation,
	options: BuilderOptions,
}

function getAsComptimeType(type: ScopeObject, location?: SourceLocation): ScopeObject & { kind: "type" } {
	if (type.kind == "type") {
		if (!location) location = "builtin";
		return {
			kind: "type",
			originLocation: location,
			name: type.name,
			comptime: true,
		}	
	} else {
		throw utilities.unreachable();
	}
}

export function getTypeDescription(type: ScopeObject & { kind: "type" }) {
	return `${type.name}`;
}

export function getTypeOf(context: BuilderContext, value: ScopeObject): ScopeObject {
	let actualType: ScopeObject;
	if (value.kind == "bool") {
		actualType = getAsComptimeType(unwrapScopeObject(getAlias(context, "Bool")));
	} else if (value.kind == "number") {
		actualType = getAsComptimeType(unwrapScopeObject(getAlias(context, "Number")));
	} else if (value.kind == "string") {
		actualType = getAsComptimeType(unwrapScopeObject(getAlias(context, "String")));
	} else if (value.kind == "type") {
		actualType = getAsComptimeType(unwrapScopeObject(getAlias(context, "Type")));
	} else if (value.kind == "complexValue") {
		actualType = value.type;
	} else {
		actualType = value;
	}
	
	return actualType;
}

function expectType(context: BuilderContext, expected: ScopeObject, actual: ScopeObject, compileError: CompileError) {
	let expectedType: ScopeObject = expected;
	
	const actualType = getTypeOf(context, actual);
	
	if (expectedType.kind == "type" && actualType.kind == "type") {
		if (expectedType == unwrapScopeObject(getAlias(context, "Any"))) {
			return;
		}
		
		if (expectedType.comptime && !actualType.comptime) {
			compileError.msg = `expected type ${getTypeDescription(expectedType)} that is a compile time type, but got type ${getTypeDescription(actualType)} that is not a compile time type`;
			throw compileError;
		}
		
		if (expectedType.name != actualType.name) {
			compileError.msg = compileError.msg
				.replace("$expectedTypeName", expectedType.name)
				.replace("$actualTypeName", actualType.name);
			throw compileError;
		}
	} else {
		utilities.unreachable();
	}
}

function getAlias(context: BuilderContext, name: string): ScopeObject & { kind: "alias" } | null {
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
			list.push(scopeObject);
		}
	}
	
	// visible
	if (context.scope.function) {
		for (let i = 0; i < context.scope.function.visible.length; i++) {
			const scopeObject = context.scope.function.visible[i];
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
			
			if (argument.kind == "argument" && argumentType.kind == "type") {
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
						name: argument.name,
						value: {
							kind: "complexValue",
							originLocation: argument.originLocation,
							type: unwrapScopeObject(argument.type),
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
		
		let result = build(context, functionToCall.AST, {
			compileTime: compileTime,
			codeGenText: text,
		}, {
			location: functionToCall.originLocation,
			msg: `function ${functionToCall.symbolName}`,
		}, true)[0];
		
		context.scope = oldScope;
		
		if (result) {
			if (functionToCall.returnType) {
				expectType(context, unwrapScopeObject(functionToCall.returnType[0]), result,
					new CompileError(`expected type $expectedTypeName but got type $actualTypeName`)
						.indicator(location, "call here")
						.indicator(functionToCall.originLocation, "function defined here")
				);	
			} else {
				throw new CompileError(`void function returned a value`)
					.indicator(location, "call here")
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
				if (callDest) codeGen.call(callDest, context, functionToCall, argumentText);
				codeGen.function(codeGen.getTop(), context, functionToCall, text);
			}
			
			if (innerDest) innerDest.push(...text);
		}
		
		return result;
	} else {
		utilities.unreachable();
		return {} as ScopeObject;
	}
}

export function build(context: BuilderContext, AST: ASTnode[], options: BuilderOptions | null, sackMarker: Indicator | null, resultAtRet?: boolean): ScopeObject[] {
	context.scope.currentLevel++;
	
	let scopeList: ScopeObject[] = [];
	
	if (context.scope.levels[context.scope.currentLevel] == undefined) {
		context.scope.levels[context.scope.currentLevel] = [];	
	}
	
	try {
		if (options) {
			const oldOptions = context.options;
			context.options = options;
			scopeList = _build(context, AST, resultAtRet == true);
			context.options = oldOptions;
		} else {
			scopeList = _build(context, AST, resultAtRet == true);
		}
	} catch (error) {
		if (error instanceof CompileError && sackMarker != null) {
			stdout.write(`stack trace ${getIndicator(sackMarker, true)}`);
		}
		context.scope.levels[context.scope.currentLevel] = [];
		context.scope.currentLevel--;
		throw error;
	}
	
	if (context.scope.currentLevel != 0) {
		context.scope.levels[context.scope.currentLevel] = [];	
	}
	context.scope.currentLevel--;
	
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
			addAlias(context, context.scope.currentLevel, {
				kind: "alias",
				originLocation: node.location,
				mutable: node.mutable,
				name: node.name,
				value: null,
				symbolName: node.name,
				type: null,
			});
		}
	}
	
	for (let index = 0; index < AST.length; index++) {
		const node = AST[index];
		
		if (node.kind == "definition") {
			const alias = getAlias(context, node.name);
			if (alias) {
				const valueText = getCGText();
				const value = unwrapScopeObject(build(context, [node.value], {
					codeGenText: valueText,
					compileTime: context.options.compileTime,
				}, null)[0]);
				
				if (!value) {
					throw new CompileError(`no value for alias`).indicator(node.location, "here");
				}
				
				if (node.value.kind == "function" && value.kind == "function" && value.originLocation != "builtin") {
					value.symbolName += `:${value.originLocation.path}:${alias.name}`;
				}
				
				const type = getTypeOf(context, value);
				
				alias.value = value;
				alias.type = type;
				
				if (doCodeGen(context)) {
					codeGen.alias(context.options.codeGenText, context, alias, valueText);
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
				const functionToCall = unwrapScopeObject(build(context, node.left, {
					compileTime: context.options.compileTime,
					codeGenText: leftText,
				}, null)[0]);
				const argumentText = getCGText();
				const callArguments = build(context, node.callArguments, {
					compileTime: context.options.compileTime,
					codeGenText: argumentText,
				}, null);
				
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
				}, null);
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
					}, null)[0];
					const rightText = getCGText();
					const right = build(context, node.right, {
						compileTime: context.options.compileTime,
						codeGenText: rightText,
					}, null)[0];
					
					if (left.kind == "alias") {
						if (!left.mutable) {
							throw new CompileError(`the alias '${left.name}' is not mutable`)
								.indicator(node.location, "reassignment here")
								.indicator(left.originLocation, "alias defined here");
						}
						
						if (doCodeGen(context)) {
							codeGen.set(context.options.codeGenText, context, left, leftText, rightText);
						}
						
						left.value = right;
					} else {
						utilities.unreachable();
					}
				}
				
				else if (node.operatorText == ".") {
					const left = unwrapScopeObject(build(context, node.left, null, null)[0]);
					
					if (left.kind == "struct" && node.right[0].kind == "identifier") {
						for (let i = 0; i < left.properties.length; i++) {
							const alias = left.properties[i];
							if (alias.kind == "alias" && alias.value) {
								if (alias.name == node.right[0].name) {
									addToScopeList(alias);
								}
							} else {
								utilities.unreachable();
							}
						}
					} else {
						utilities.unreachable();
					}
				}
				
				else {
					const left = unwrapScopeObject(build(context, node.left, null, null)[0]);
					const right = unwrapScopeObject(build(context, node.right, null, null)[0]);
					
					if (left.kind == "number" && right.kind == "number") {
						if (node.operatorText == "+") {
							addToScopeList({
								kind: "number",
								originLocation: node.location,
								value: left.value + right.value,
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
						}
					}
				}
				
				break;
			}
			case "comptime": {
				const value = unwrapScopeObject(build(context, [node.value], null, null)[0]);
				if (value.kind == "type") {
					addToScopeList(getAsComptimeType(value, node.location));
				} else {
					utilities.unreachable();
				}
				break;
			}
			
			case "function": {
				let returnType = null;
				if (node.returnType) {
					returnType = build(context, [node.returnType.value], null, null);
				}
				
				let functionArguments: (ScopeObject & { kind: "argument" })[] = [];
				
				for (let i = 0; i < node.functionArguments.length; i++) {
					const argument = node.functionArguments[i];
					
					if (argument.kind == "argument") {
						const argumentType = build(context, [argument.type.value], {
							codeGenText: null,
							compileTime: true,
						}, null)[0];
						
						functionArguments.push({
							kind: "argument",
							originLocation: argument.location,
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
					originLocation: node.location,
					symbolName: `${nextSymbolName}`,
					functionArguments: functionArguments,
					returnType: returnType,
					AST: node.codeBlock,
					visible: visible,
				});
				nextSymbolName++;
				break;
			}
			case "struct": {
				break;
			}
			case "codeGenerate": {
				build(context, node.codeBlock, {
					codeGenText: context.options.codeGenText,
					compileTime: false,
				}, null);
				break;
			}
			case "while": {
				while (true) {
					const condition = build(context, node.condition, null, null)[0];
					
					if (condition.kind == "bool") {
						if (condition.value) {
							build(context, node.codeBlock, null, null, resultAtRet)[0];
						} else {
							break;
						}
					} else {
						utilities.unreachable();
					}	
				}
				break;
			}
			case "if": {
				const condition = unwrapScopeObject(build(context, node.condition, null, null)[0]);
				
				if (condition.kind == "bool") {
					if (condition.value) {
						build(context, node.trueCodeBlock, null, null, resultAtRet)[0];
					} else {
						if (node.falseCodeBlock) {
							build(context, node.falseCodeBlock, null, null, resultAtRet)[0];
						}
					}
				} else {
					utilities.unreachable();
				}	
				break;
			}
			case "return": {
				if (!resultAtRet) {
					throw new CompileError("unexpected return").indicator(node.location, "here");
				}
				if (node.value) {
					const value = build(context, [node.value], null, null)[0];
					scopeList.push(value);	
				} else {
					
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
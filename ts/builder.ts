import { stdout } from "process";

import {
	SourceLocation,
	ASTnode,
	ScopeObject,
	unwrapScopeObject,
} from "./types";
import utilities from "./utilities";
import { Indicator, getIndicator, CompileError } from "./report";
import { builtinScopeLevel, builtinCall, onCodeGen, getString } from "./builtin";
import codeGen from "./codeGen";

let nextSymbolName = 0;

export type ScopeInformation = {
	levels: ScopeObject[][],
	currentLevel: number,
	visible: ScopeObject[],
}

export type BuilderOptions = {
	doCodeGen: boolean,
}

export type BuilderContext = {
	codeGenText: any,
	
	filePath: string,
	
	scope: ScopeInformation,
	
	options: BuilderOptions,
}

function expectType(context: BuilderContext, expected: ScopeObject, actual: ScopeObject, compileError: CompileError) {
	let expectedType: ScopeObject = expected;
	
	let actualType: ScopeObject = {} as ScopeObject;
	if (actual.kind == "bool") {
		const scopeObject = getAlias(context, "Bool");
		if (scopeObject && scopeObject.kind == "alias" && scopeObject.value) {
			actualType = scopeObject.value;	
		}
	} else if (actual.kind == "number") {
		const scopeObject = getAlias(context, "Number");
		if (scopeObject && scopeObject.kind == "alias" && scopeObject.value) {
			actualType = scopeObject.value;	
		}
	} else if (actual.kind == "string") {
		const scopeObject = getAlias(context, "String");
		if (scopeObject && scopeObject.kind == "alias" && scopeObject.value) {
			actualType = scopeObject.value;	
		}
	} else {
		actualType = actual;
	}
	
	if (expectedType.kind == "type" && actualType.kind == "type") {
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

function getAlias(context: BuilderContext, name: string): ScopeObject | null {
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
	for (let i = 0; i < context.scope.visible.length; i++) {
		const scopeObject = context.scope.visible[i];
		if (scopeObject.kind == "alias") {
			if (scopeObject.name == name) {
				return scopeObject;
			}
		} else {
			utilities.unreachable();
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
	for (let i = 0; i < context.scope.visible.length; i++) {
		const scopeObject = context.scope.visible[i];
		list.push(scopeObject);
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

export function callFunction(context: BuilderContext, functionToCall: ScopeObject, callArguments: ScopeObject[], location: SourceLocation, fillComplex: boolean, doCodeGen: boolean): ScopeObject {
	if (functionToCall.kind == "function") {
		if (!fillComplex) {
			if (callArguments.length > functionToCall.functionArguments.length) {
				throw new CompileError(`too many arguments passed to function '${functionToCall.name}'`)
					.indicator(location, "function call here");
			}
			
			if (callArguments.length < functionToCall.functionArguments.length) {
				throw new CompileError(`not enough arguments passed to function '${functionToCall.name}'`)
					.indicator(location, "function call here");
			}	
		}
		
		const oldScope = context.scope;
		context.scope = {
			currentLevel: -1,
			levels: [[]],
			visible: functionToCall.visible,
		}
		
		for (let index = 0; index < functionToCall.functionArguments.length; index++) {
			const argument = functionToCall.functionArguments[index];
			
			const callArgument = unwrapScopeObject(callArguments[index]);
			
			if (argument.kind == "argument") {
				let generatedName = null;
				const originalAlias = callArguments[index];
				if (originalAlias.kind == "alias") {
					generatedName = originalAlias.generatedName
				} else if (doCodeGen) {
					generatedName = argument.name;
				}
				
				if (fillComplex) {
					addAlias(context, context.scope.currentLevel + 1, {
						kind: "alias",
						originLocation: argument.originLocation,
						mutable: false,
						name: argument.name,
						value: {
							kind: "complexValue",
							originLocation: argument.originLocation,
							type: argument.type,
						},
						generatedName: generatedName,
					});
				} else {
					expectType(context, unwrapScopeObject(argument.type[0]), callArgument,
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
						generatedName: generatedName,
					});	
				}
			} else {
				utilities.unreachable();
			}
		}
		
		const result = build(context, functionToCall.AST, {
			doCodeGen: doCodeGen,
		}, {
			location: functionToCall.originLocation,
			msg: `function ${functionToCall.name}`,
		}, true)[0];
		
		context.scope = oldScope;
		
		if (result && functionToCall.returnType) {
			expectType(context, functionToCall.returnType[0], result,
				new CompileError(`expected type $expectedTypeName but got type $actualTypeName`)
					.indicator(location, "call here")
					.indicator(functionToCall.originLocation, "function defined here")
			);
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
				generatedName: codeGen.getNewName(context, node.name),
			});
		}
	}
	
	for (let index = 0; index < AST.length; index++) {
		const node = AST[index];
		
		if (node.kind == "definition") {
			const alias = getAlias(context, node.name);
			if (alias && alias.kind == "alias") {
				const value = build(context, [node.value], null, null)[0];
				
				if (!value) {
					throw new CompileError(`no value for alias`).indicator(node.location, "here");
				}
				
				if (node.value.kind == "function" && value.kind == "function" && value.originLocation != "builtin") {
					value.name += `:${value.originLocation.path}:${alias.name}`;
				}
				
				alias.value = value;
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
				break;
			}
			
			case "identifier": {
				const alias = getAlias(context, node.name);
				if (alias && alias.kind == "alias") {
					if (alias.value) {
						if (alias.generatedName) {
							codeGen.identifier(context, alias);
						}
						
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
				const functionToCall = unwrapScopeObject(build(context, node.left, null, null)[0]);
				const callArguments = build(context, node.callArguments, {
					doCodeGen: false,
				}, null);
				
				if (functionToCall.kind == "function") {
					const result = callFunction(context, functionToCall, callArguments, node.location, false, false);
					
					addToScopeList(result);
				} else {
					throw new CompileError(`attempting a function call on something other than a function`)
						.indicator(node.location, "here");
				}
				break;
			}
			case "builtinCall": {
				const callArguments = build(context, node.callArguments, null, null);
				const result = builtinCall(context, node, callArguments);
				if (result) {
					addToScopeList(result);
				}
				break;
			}
			case "operator": {
				if (node.operatorText == "=") {
					const left = build(context, node.left, null, null)[0];
					const right = build(context, node.right, null, null)[0];
					
					if (left.kind == "alias") {
						if (!left.mutable) {
							throw new CompileError(`the alias '${left.name}' is not mutable`)
								.indicator(node.location, "reassignment here")
								.indicator(left.originLocation, "alias defined here");
						}
						
						left.value = right;
					} else {
						utilities.unreachable();
					}
				}
				
				else if (node.operatorText == ".") {
					const left = build(context, node.left, null, null)[0];
					
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
					const left = build(context, node.left, null, null)[0];
					const right = build(context, node.right, null, null)[0];
					
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
			
			case "function": {
				let returnType = null;
				if (node.returnType) {
					returnType = build(context, node.returnType, null, null);
				}
				
				let functionArguments: ScopeObject[] = [];
				
				for (let i = 0; i < node.functionArguments.length; i++) {
					const argument = node.functionArguments[i];
					
					if (argument.kind == "argument") {
						const argumentType = build(context, argument.type, null, null);
						
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
					originLocation: node.location,
					name: `${nextSymbolName}`,
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
					doCodeGen: true,
				}, null, resultAtRet);
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
				const condition = build(context, node.condition, null, null)[0];
				
				if (condition.kind == "bool") {
					if (condition.value) {
						build(context, node.codeBlock, null, null, resultAtRet)[0];
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
				const value = build(context, node.value, null, null)[0];
				scopeList.push(value);
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
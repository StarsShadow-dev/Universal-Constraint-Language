import { stdout } from "process";

import {
	SourceLocation,
	ASTnode,
	ScopeObject,
} from "./types";
import utilities from "./utilities";
import { Indicator, getIndicator, CompileError } from "./report";
import { builtinScopeLevel, builtinCall } from "./builtin";

let nextSymbolName = 0;

export type BuilderContext = {
	scopeLevels: ScopeObject[][],
	level: number,
	codeGenText: any,
	filePath: string,
}

export type BuilderOptions = {
	getAlias: boolean,
	resultAtRet: boolean,
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
	{
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
	}
	for (let level = context.level; level >= 0; level--) {
		for (let i = 0; i < context.scopeLevels[level].length; i++) {
			const scopeObject = context.scopeLevels[level][i];
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

function addAlias(context: BuilderContext, level: number, alias: ScopeObject) {
	if (alias.kind == "alias") {
		const oldAlias = getAlias(context, alias.name)
		if (oldAlias) {
			throw new CompileError(`alias '${alias.name}' already exists`)
				.indicator(oldAlias.originLocation, "alias originally defined here")
				.indicator(alias.originLocation, "alias redefined here");
		}
		context.scopeLevels[level].push(alias);
	} else {
		utilities.unreachable();
	}
}

export function callFunction(context: BuilderContext, functionToCall: ScopeObject, callArguments: ScopeObject[], location: SourceLocation, fillComplex: boolean): ScopeObject {
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
		
		for (let index = 0; index < functionToCall.functionArguments.length; index++) {
			const argument = functionToCall.functionArguments[index];
			
			if (argument.kind == "argument") {
				if (fillComplex) {
					addAlias(context, context.level + 1, {
						kind: "alias",
						originLocation: argument.originLocation,
						mutable: false,
						name: argument.name,
						value: {
							kind: "complexValue",
							originLocation: argument.originLocation,
							type: argument.type,
						},
					});
				} else {
					expectType(context, argument.type[0], callArguments[index],
						new CompileError(`expected type $expectedTypeName but got type $actualTypeName`)
							.indicator(callArguments[index].originLocation, "argument here")
							.indicator(argument.originLocation, "argument defined here")
					);
					
					addAlias(context, context.level + 1, {
						kind: "alias",
						originLocation: argument.originLocation,
						mutable: false,
						name: argument.name,
						value: callArguments[index],
					});		
				}
			} else {
				utilities.unreachable();
			}
		}
		
		const result = build(context, functionToCall.AST, {
			getAlias: false,
			resultAtRet: true,
		}, {
			location: functionToCall.originLocation,
			msg: `function ${functionToCall.name}`,
		})[0];
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
	}
	
	return {} as ScopeObject;
}

export function build(context: BuilderContext, AST: ASTnode[], options: BuilderOptions | null, sackMarker: Indicator | null): ScopeObject[] {
	context.level++;
	
	let scopeList: ScopeObject[] = [];
	
	if (context.scopeLevels[context.level] == undefined) {
		context.scopeLevels[context.level] = [];	
	}
	
	try {
		if (options) {
			scopeList = _build(context, AST, options);	
		} else {
			scopeList = _build(context, AST, {
				getAlias: false,
				resultAtRet: false,
			});
		}
	} catch (error) {
		if (error instanceof CompileError && sackMarker != null) {
			stdout.write(`stack trace ${getIndicator(sackMarker, true)}`);
		}
		context.scopeLevels[context.level] = [];
		context.level--;
		throw error;
	}
	
	if (context.level != 0) {
		context.scopeLevels[context.level] = [];	
	}
	context.level--;
	
	return scopeList;
}

export function _build(context: BuilderContext, AST: ASTnode[], options: BuilderOptions): ScopeObject[] {
	let scopeList: ScopeObject[] = [];
	
	function addToScopeList(scopeObject: ScopeObject) {
		if (!options.resultAtRet) {
			scopeList.push(scopeObject);	
		}
	}
	
	for (let i = 0; i < AST.length; i++) {
		const node = AST[i];
		
		if (node.kind == "definition") {
			addAlias(context, context.level, {
				kind: "alias",
				originLocation: node.location,
				mutable: node.mutable,
				name: node.name,
				value: null,
			});
		}
	}
	
	for (let index = 0; index < AST.length; index++) {
		const node = AST[index];
		
		if (node.kind == "definition") {
			const alias = getAlias(context, node.name);
			if (alias && alias.kind == "alias") {
				const value = build(context, [node.value], null, null)[0];
				
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
						if (options.getAlias) {
							addToScopeList(alias);	
						} else {
							addToScopeList(alias.value);
						}
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
				const functionToCall = build(context, node.left, null, null)[0];
				const callArguments = build(context, node.callArguments, null, null);
				
				const result = callFunction(context, functionToCall, callArguments, node.location, false);
				
				addToScopeList(result);
				
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
					const left = build(context, node.left, {
						getAlias: true,
						resultAtRet: false,
					}, null)[0];
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
									if (options.getAlias) {
										addToScopeList(alias);	
									} else {
										addToScopeList(alias.value);
									}
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
			
			case "assignment": {
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
				
				addToScopeList({
					kind: "function",
					originLocation: node.location,
					name: `${nextSymbolName}`,
					functionArguments: functionArguments,
					returnType: returnType,
					AST: node.codeBlock,
				});
				nextSymbolName++;
				break;
			}
			case "struct": {
				break;
			}
			case "while": {
				while (true) {
					const condition = build(context, node.condition, null, null)[0];
					
					if (condition.kind == "bool") {
						if (condition.value) {
							build(context, node.codeBlock, null, null)[0];
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
				break;
			}
			case "return": {
				if (!options.resultAtRet) {
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
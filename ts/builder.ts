import { stdout } from 'process';

import {
	SourceLocation,
	ASTnode,
	ScopeObject,
} from "./types";
import utilities from "./utilities";
import { CompileError } from "./report";

// builtin constructor
class BC {
	private builtinNode: ASTnode
	private scopeObjects: ScopeObject[]
	private nodes: ASTnode[]
	private i: number
	
	constructor(builtinNode: ASTnode, scopeObjects: ScopeObject[], nodes: ASTnode[]) {
		this.builtinNode = builtinNode;
		this.scopeObjects = scopeObjects;
		this.nodes = nodes;
		this.i = 0;
	}
	
	public "string" = (): string => {
		const scopeObject = this.scopeObjects[this.i];
		const node = this.nodes[this.i];
		
		if (!scopeObject || !node) {
			throw new CompileError("builtin argument error")
				.indicator(this.builtinNode.location, "expected a string but there are no more arguments");
		}
		
		this.i++;
		
		if (scopeObject.type == "string") {
			return scopeObject.value;
		} else {
			throw new CompileError("builtin argument error")
				.indicator(node.location, "expected a string");
		}
	}
	
	public done() {
		const node = this.nodes[this.i];
		
		if (node) {
			throw new CompileError("builtin argument error")
				.indicator(node.location, "expected no more arguments");
		}
	}
	
	public next(): ScopeObject {
		const scopeObject = this.scopeObjects[this.i];
		return scopeObject;
	}
}

export type BuilderContext = {
	scopeLevels: ScopeObject[][],
	level: number,
}

function getScopeObject(context: BuilderContext, name: string): ScopeObject | null {
	for (let level = context.level; level >= 0; level--) {
		for (let i = 0; i < context.scopeLevels[level].length; i++) {
			const scopeObject = context.scopeLevels[level][i];
			if (scopeObject.type == "alias") {
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

export function build(context: BuilderContext, AST: ASTnode[], sackMarker: {name: string, location: SourceLocation} | null): ScopeObject[] {
	context.level++;
	
	let scopeList: ScopeObject[] = [];
	
	try {
		scopeList = _build(context, AST);
	} catch (error) {
		console.log("error", error);
		throw error;
	}
	
	context.level--;
	
	return scopeList;
}

export function _build(context: BuilderContext, AST: ASTnode[]): ScopeObject[] {
	let scopeList: ScopeObject[] = [];
	
	for (let i = 0; i < AST.length; i++) {
		const node = AST[i];
		
		if (node.type == "definition") {
			context.scopeLevels[context.level].push({
				type: "alias",
				originLocation: node.location,
				mutable: node.mutable,
				name: node.name,
				value: null,
			});	
		}
	}
	
	for (let i = 0; i < AST.length; i++) {
		const node = AST[i];
		
		if (node.type == "definition") {
			const alias = getScopeObject(context, node.name);
			if (alias && alias.type == "alias") {
				const value = build(context, node.value, null);
				
				alias.value = value;
			} else {
				utilities.unreachable();
			}
			continue;
		}
		
		switch (node.type) {
			case "bool": {
				break;
			}
			case "number": {
				scopeList.push({
					type: "number",
					originLocation: node.location,
					value: node.value,
				});
				break;
			}
			case "string": {
				scopeList.push({
					type: "string",
					originLocation: node.location,
					value: node.value,
				});
				break;
			}
			
			case "identifier": {
				const alias = getScopeObject(context, node.name);
				if (alias && alias.type == "alias") {
					if (alias.value) {
						// scopeList.push(alias);
						scopeList.push(alias.value[0]);
					} else {
						throw new CompileError("alias used before its definition")
							.indicator(node.location, "identifier here")
							.indicator(alias.originLocation, "alias defined here");
					}
				} else {
					throw new CompileError("alias does not exist").indicator(node.location, "here");
				}
				break;
			}
			case "call": {
				break;
			}
			case "builtinCall": {
				const callArguments = build(context, node.callArguments, null);
				
				const bc = new BC(node, callArguments, node.callArguments);
				
				if (node.name == "compileLog") {
					let str = "[compileLog]";
					while (bc.next()) {
						str += " " + bc.string();
					}
					str += "\n"
					bc.done();
					
					stdout.write(str);
				}
				break;
			}
			
			case "assignment": {
				break;
			}
			case "function": {
				break;
			}
			case "struct": {
				break;
			}
			case "while": {
				break;
			}
			case "if": {
				break;
			}
			case "return": {
				break;
			}
		
			default: {
				utilities.unreachable();
				break;
			}
		}
	}
	
	context.level--;
	return scopeList;
}
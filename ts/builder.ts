import {
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
			new CompileError("builtin argument error")
				.indicator(this.builtinNode.location, "expected a string but there are no more arguments")
				.fatal();
		}
		
		this.i++;
		
		if (scopeObject.type == "string") {
			return scopeObject.value;
		} else {
			new CompileError("builtin argument error")
				.indicator(node.location, "expected a string")
				.fatal();
		}
		
		return "";
	}
	
	public done() {
		const node = this.nodes[this.i];
		
		if (node) {
			new CompileError("builtin argument error")
				.indicator(node.location, "expected no more arguments")
				.fatal();
		}
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

export function build(context: BuilderContext, AST: ASTnode[]): ScopeObject[] {
	context.level++;
	
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
				const value = build(context, node.value);
				
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
						new CompileError("alias used before its definition")
							.indicator(node.location, "identifier here")
							.indicator(alias.originLocation, "alias defined here")
							.fatal();
					}
				} else {
					new CompileError("alias does not exist").indicator(node.location, "here").fatal();
				}
				break;
			}
			case "call": {
				break;
			}
			case "builtinCall": {
				const callArguments = build(context, node.callArguments);
				
				const bc = new BC(node, callArguments, node.callArguments);
				
				if (node.name == "compileLog") {
					console.log("[compileLog]",  bc.string());
					bc.done();
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
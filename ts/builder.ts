import {
	ASTnode,
	ScopeObject,
} from "./types";
import utilities from "./utilities";
import { compileError } from "./report";

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
			const value = build(context, node.value);
			
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
			}
			case "string": {
				break;
			}
			
			case "identifier": {
				const alias = getScopeObject(context, node.name);
				if (alias && alias.type == "alias") {
					if (alias.value) {
						// scopeList.push(alias);
						scopeList.push(alias.value[0]);
					} else {
						compileError(node.location, "alias used before its definition", "");
					}
				} else {
					compileError(node.location, "alias does not exist", "");
				}
				break;
			}
			case "call": {
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
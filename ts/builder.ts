import {
	ASTnode,
	ScopeObject,
} from "./types";
import utilities from "./utilities";
import { compileError } from "./report";

export function build(AST: ASTnode[]): ScopeObject[] {
	let scopeList: ScopeObject[] = [];
	
	for (let i = 0; i < AST.length; i++) {
		const node = AST[i];
		
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
				break;
			}
			case "call": {
				break;
			}
			
			case "assignment": {
				compileError(node.location, "assignment!", "");
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
	
	return scopeList;
}
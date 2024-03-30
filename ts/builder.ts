import {
	ASTnode,
	ScopeObject,
} from "./types";

export function build(AST: ASTnode[]): ScopeObject[] {
	let scopeList: ScopeObject[] = [];
	
	for (let i = 0; i < AST.length; i++) {
		const node = AST[i];
		
		if (node.type == "number") {
			scopeList.push({
				type: "number",
				originLocation: node.location,
				value: node.value,
			});
		}
	}
	
	return scopeList;
}
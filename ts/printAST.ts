import { ASTnode, ASTnode_isAtype } from "./parser";
import utilities from "./utilities";

export function printAST(AST: ASTnode[]): string {
	let text = "";
	for (let i = 0; i < AST.length; i++) {
		text += printASTnode(AST[i]);
	}
	return text;
}

export function printASTnode(node: ASTnode): string {
	switch (node.kind) {
		case "bool": {
			if (node.value) {
				return "true";
			} else {
				return "false";
			}
		}
		case "number": {
			return `${node.value}`;
		}
		case "string": {
			return `"${node.value}"`;
		}
		case "identifier": {
			return node.name;
		}
		case "instance": {
			return `${printASTnode(node.template)} {}`;
		}
	}
	throw utilities.TODO();
}
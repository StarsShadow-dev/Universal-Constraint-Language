import { ASTnode } from "./parser";
import utilities from "./utilities";

export function printAST(AST: ASTnode[]): string {
	let text = "";
	for (let i = 0; i < AST.length; i++) {
		text += printASTnode(AST[i]);
	}
	return text;
}

export function printASTnode(ASTnode: ASTnode): string {
	switch (ASTnode.kind) {
		case "bool": {
			if (ASTnode.value) {
				return "true";
			} else {
				return "false";
			}
		}
		case "number": {
			return `${ASTnode.value}`;
		}
		case "string": {
			return `"${ASTnode.value}"`;
		}
	}
	throw utilities.TODO();
}
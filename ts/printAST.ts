import utilities from "./utilities";
import { ASTnode } from "./parser";

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
		case "call": {
			let argText = "";
			for (let i = 0; i < node.callArguments.length; i++) {
				const argNode = node.callArguments[i];
				const comma = argText == "";
				argText += printASTnode(argNode);
				if (comma) {
					argText += ", ";
				}
			}
			return `${printASTnode(node.left)}(${argText})`;
		}
		case "builtinCall": {
			let argText = "";
			for (let i = 0; i < node.callArguments.length; i++) {
				const argNode = node.callArguments[i];
				const comma = argText == "";
				argText += printASTnode(argNode);
				if (comma) {
					argText += ", ";
				}
			}
			return `@${node.name}(${argText})`;
		}
		case "struct": {
			if (node.id.startsWith("builtin:")) {
				return node.id.split(":")[1];
			}
			
			let argText = "";
			for (let i = 0; i < node.fields.length; i++) {
				const argNode = node.fields[i];
				const comma = argText == "";
				argText += printASTnode(argNode);
				if (comma) {
					argText += ", ";
				}
			}
			return `struct(${argText})`;
		}
		case "instance": {
			return `${printASTnode(node.template)} {}`;
		}
		case "effect": {
			return `effect ${printASTnode(node.type)}`;
		}
	}
	throw utilities.TODO();
}
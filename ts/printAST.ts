import utilities from "./utilities";
import { ASTnode } from "./parser";

function getArgText(args: ASTnode[]): string {
	let argText = "";
	for (let i = 0; i < args.length; i++) {
		const argNode = args[i];
		const comma = argText == "";
		argText += printASTnode(argNode);
		if (comma) {
			argText += ", ";
		}
	}
	return argText;
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
			const argText = getArgText(node.callArguments);
			return `${printASTnode(node.left)}(${argText})`;
		}
		case "builtinCall": {
			const argText = getArgText(node.callArguments);
			return `@${node.name}(${argText})`;
		}
		case "struct": {
			if (node.id.startsWith("builtin:")) {
				return node.id.split(":")[1];
			}
			
			const argText = getArgText(node.fields);
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

export function printAST(AST: ASTnode[]): string {
	let text = "";
	for (let i = 0; i < AST.length; i++) {
		text += printASTnode(AST[i]);
	}
	return text;
}
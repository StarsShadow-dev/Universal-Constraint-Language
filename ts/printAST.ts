import utilities from "./utilities";
import { ASTnode } from "./parser";

export type CodeGenContext = {
	level: number,
};

function getArgText(context: CodeGenContext, args: ASTnode[]): string {
	let argText = "";
	for (let i = 0; i < args.length; i++) {
		const argNode = args[i];
		const comma = argText == "";
		argText += printASTnode(context, argNode);
		if (comma) {
			argText += ", ";
		}
	}
	return argText;
}

export function printASTnode(context: CodeGenContext, node: ASTnode): string {
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
			const argText = getArgText(context, node.callArguments);
			return `${printASTnode(context, node.left)}(${argText})`;
		}
		case "builtinCall": {
			const argText = getArgText(context, node.callArguments);
			return `@${node.name}(${argText})`;
		}
		case "struct": {
			if (node.id.startsWith("builtin:")) {
				return node.id.split(":")[1];
			}
			
			const argText = getArgText(context, node.fields);
			return `struct(${argText})`;
		}
		case "functionType": {
			const argText = getArgText(context, node.functionArguments);
			const returnType = printASTnode(context, node.returnType);
			return `\(${argText}) -> ${returnType}`;
		}
		case "function": {
			let argText = "";
			for (let i = 0; i < node.functionArguments.length; i++) {
				const arg = node.functionArguments[i];
				argText += `${arg.name}: ${printASTnode(context, arg.type)}`;
			}
			const returnType = printASTnode(context, node.returnType);
			const codeBlock = printAST(context, node.codeBlock);
			return `fn(${argText}) -> ${returnType} {${codeBlock}}`;
		}
		case "instance": {
			// TODO
			return `${printASTnode(context, node.template)} {}`;
		}
		case "alias": {
			const value = printASTnode(context, node.value);
			return `${node.name} = ${value}`;
		}
	}
	throw utilities.TODO();
}

export function printAST(context: CodeGenContext, AST: ASTnode[]): string {
	let text = "";
	
	context.level++;
	for (let i = 0; i < AST.length; i++) {
		text += "\n" + "\t".repeat(context.level) + printASTnode(context, AST[i]);
	}
	context.level--;
	
	return text + "\n" + "\t".repeat(context.level);
}
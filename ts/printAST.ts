import * as utilities from "./utilities";
import { ASTnode } from "./parser";

type CodeGenContext = {
	level: number,
};

export function getCodeGenContext(): CodeGenContext {
	return {
		level: 0,
	}
}

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
			const left = printASTnode(context, node.left);
			const arg = printASTnode(context, node.arg);
			return `(${left} ${arg})`;
		}
		case "operator": {
			const left = printASTnode(context, node.left);
			const right = printASTnode(context, node.right);
			return `(${left} ${node.operatorText} ${right})`;
		}
		case "argument": {
			const type = printASTnode(context, node.type);
			return `${node.name}: ${type}`;
		}
		case "struct": {
			if (node.id.startsWith("builtin:")) {
				return node.id.split(":")[1];
			}
			
			const argText = getArgText(context, node.fields);
			return `struct(${argText})`;
		}
		case "functionType": {
			const argType = printASTnode(context, node.arg);
			const returnType = printASTnode(context, node.returnType);
			return `\\${argType} -> ${returnType}`;
		}
		case "function": {
			let type = printASTnode(context, node.arg.type);
			let body = "";
			if (node.body.length == 1) {
				body = printASTnode(context, node.body[0]);
			} else {
				body = printAST(context, node.body);
			}
			return `@${node.arg.name}(${type}) ${body}`;
		}
		// case "if": {
		// 	const condition = printASTnode(context, node.condition);
		// 	const trueCodeBlock = printAST(context, node.trueBody);
		// 	const falseCodeBlock = printAST(context, node.falseBody);
		// 	return `if (${condition}) {${trueCodeBlock}} else {${falseCodeBlock}}`;
		// }
		case "instance": {
			const codeBlock = printAST(context, node.codeBlock);
			return `${printASTnode(context, node.template)} {${codeBlock}}`;
		}
		case "alias": {
			const value = printASTnode(context, node.value);
			return `${node.name} = ${value}`;
		}
		case "_selfType": {
			return `___selfType___`;
		}
	}
	utilities.TODO();
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

export function justPrint(ASTnode: ASTnode) { return printASTnode(getCodeGenContext(), ASTnode) }
export function justPrintList(AST: ASTnode[]) { return printAST(getCodeGenContext(), AST) }
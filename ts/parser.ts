import {
	TokenType,
	Token,
	
	ASTnode,
	AssignmentType,
} from "./types";
import { compileError } from "./report";
import utilities from "./utilities";

function needsSemicolon(node: ASTnode): boolean {
	return true;
}

export type ParserContext = {
	tokens: Token[],
	i: number,
}

export enum ParserMode {
	normal,
	single,
}

export function parse(context: ParserContext, mode: ParserMode): ASTnode[] {
	let AST: ASTnode[] = [];
	
	function forward(): Token {
		const token = context.tokens[context.i];
		context.i++
		return token;
	}
	
	while (context.i < context.tokens.length) {
		const token = forward();
		
		switch (token.type) {
			case TokenType.number: {
				AST.push({
					type: "number",
					location: token.location,
					value: Number(token.text),
				});
				break;
			}
			
			case TokenType.string: {
				break;
			}
			
			case TokenType.word: {
				debugger;
				if (token.text == "const") {
					const left = parse(context, ParserMode.single);
					const equals = forward();
					if (equals.type != TokenType.operator || equals.text != "=") {
						compileError(equals.location, "expected equals");
					}
					const right = parse(context, ParserMode.single);
					
					AST.push({
						type: "assignment",
						location: token.location,
						assignmentType: AssignmentType.constant,
						left: left,
						right: right,
					});
				} else {
					AST.push({
						type: "identifier",
						location: token.location,
						name: token.text,
					});
				}
				break;
			}
			
			case TokenType.separator: {
				compileError(token.location, "unexpected separator");
				break;
			}
		
			default: {
				utilities.unreachable();
				break;
			}
		}
		
		if (mode == ParserMode.single) {
			return AST;
		}
		
		if (needsSemicolon(AST[AST.length - 1])) {
			const semicolon = forward();
			if (semicolon.type != TokenType.separator || semicolon.text != ";") {
				compileError(semicolon.location, "expected a semicolon");
			}
		}
	}
	
	return AST;
}
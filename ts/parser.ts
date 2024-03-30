import {
	TokenType,
	Token,
	
	ASTnode,
} from "./types";
import { CompileError } from "./report";
import utilities from "./utilities";

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
		
		let needsSemicolon = true;
		
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
				if (token.text == "const") {
					const name = forward();
					if (name.type != TokenType.word) {
						new CompileError("expected name").indicator(name.location, "here").fatal();
					}
					
					const equals = forward();
					if (equals.type != TokenType.operator || equals.text != "=") {
						new CompileError("expected equals").indicator(equals.location, "here").fatal();
					}
					
					const value = parse(context, ParserMode.single);
					
					AST.push({
						type: "definition",
						location: token.location,
						mutable: false,
						name: name.text,
						value: value,
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
				new CompileError("unexpected separator").indicator(token.location, "here").fatal();
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
		
		if (needsSemicolon) {
			const semicolon = forward();
			if (semicolon.type != TokenType.separator || semicolon.text != ";") {
				new CompileError("expected a semicolon").indicator(semicolon.location, "here").fatal();
			}
		}
	}
	
	return AST;
}
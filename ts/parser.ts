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

export function parse(context: ParserContext, mode: ParserMode, endAt: ")" | "}" | "]" | null): ASTnode[] {
	let AST: ASTnode[] = [];
	
	function forward(): Token {
		const token = context.tokens[context.i];
		
		if (!token) {
			new CompileError("unexpected end of file").indicator(context.tokens[context.i-1].location, "last token here").fatal();	
		}
		
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
				AST.push({
					type: "string",
					location: token.location,
					value: token.text,
				});
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
					
					const value = parse(context, ParserMode.single, null);
					
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
				if (token.text == endAt) {
					return AST;
				}
				new CompileError("unexpected separator").indicator(token.location, "here").fatal();
				break;
			}
			
			case TokenType.builtinIndicator: {
				const name = forward();
				if (name.type != TokenType.word) {
					new CompileError("expected name").indicator(name.location, "here").fatal();
				}
				
				const openingParentheses = forward();
				if (openingParentheses.type != TokenType.separator || openingParentheses.text != "(") {
					new CompileError("expected openingParentheses").indicator(openingParentheses.location, "here").fatal();
				}
				
				const callArguments = parse(context, ParserMode.normal, ")");
				
				AST.push({
					type: "builtinCall",
					location: name.location,
					name: name.text,
					callArguments: callArguments,
				});
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
		
		if (endAt != null) {
			const atToken = context.tokens[context.i];
			if (atToken.type == TokenType.separator) {
				if (atToken.text == ")" || atToken.text == "}" || atToken.text == "]") {
					if (endAt == atToken.text) {
						context.i++;
						return AST;
					} else {
						new CompileError("unexpected separator")
							.indicator(atToken.location, `expected '${endAt} but got '${atToken.text}'`)
							.fatal();
					}
				}
			}
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
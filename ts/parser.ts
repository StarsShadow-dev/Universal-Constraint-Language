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
	comma,
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
				}
				
				else if (token.text == "fn") {
					const openingParentheses = forward();
					if (openingParentheses.type != TokenType.separator || openingParentheses.text != "(") {
						new CompileError("expected openingParentheses").indicator(openingParentheses.location, "here").fatal();
					}
					
					const functionArguments = parse(context, ParserMode.comma, ")");
					
					const openingBracket = forward();
					if (openingBracket.type != TokenType.separator || openingBracket.text != "{") {
						new CompileError("expected openingBracket").indicator(openingBracket.location, "here").fatal();
					}
					
					const codeBlock = parse(context, ParserMode.normal, "}");
					
					AST.push({
						type: "function",
						location: token.location,
						functionArguments: functionArguments,
						codeBlock: codeBlock,
					});
				}
				
				else {
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
				
				const callArguments = parse(context, ParserMode.comma, ")");
				
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
		
		const next = context.tokens[context.i];
		
		if (mode == ParserMode.single) {
			return AST;
		}
		
		if (endAt != null) {
			if (next.type == TokenType.separator) {
				if (next.text == ")" || next.text == "}" || next.text == "]") {
					if (endAt == next.text) {
						context.i++;
						return AST;
					} else {
						new CompileError("unexpected separator")
							.indicator(next.location, `expected '${endAt} but got '${next.text}'`)
							.fatal();
					}
				}
			}
		}
		
		if (mode == ParserMode.comma) {
			const comma = forward();
			if (comma.type != TokenType.separator || comma.text != ",") {
				new CompileError("expected a comma").indicator(comma.location, "here").fatal();
			}
			needsSemicolon = false;
		}
		
		if (next.type == TokenType.separator && next.text == "(") {
			const left = AST.pop();
			if (left != undefined) {
				forward();
				const callArguments = parse(context, ParserMode.comma, ")");
				
				AST.push({
					type: "call",
					location: next.location,
					left: [left],
					callArguments: callArguments,
				});
			} else {
				utilities.unreachable();
			}
		}
		
		if (needsSemicolon) {
			if (!next) {
				new CompileError(`expected a semicolon but the file ended`).indicator(context.tokens[context.i-1].location, "here").fatal();
			}
			const semicolon = forward();
			if (semicolon.type != TokenType.separator || semicolon.text != ";") {
				new CompileError(`expected a semicolon but got '${semicolon.text}'`).indicator(semicolon.location, "here").fatal();
			}
		}
	}
	
	return AST;
}
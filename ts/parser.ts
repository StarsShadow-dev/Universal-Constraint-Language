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

function parseFunctionArguments(context: ParserContext): ASTnode[] {
	let AST: ASTnode[] = [];
	
	function forward(): Token {
		const token = context.tokens[context.i];
		
		if (!token) {
			throw new CompileError("unexpected end of file in function arguments")
				.indicator(context.tokens[context.i-1].location, "last token here");	
		}
		
		context.i++
		return token;
	}
	
	function next(): Token {
		const token = context.tokens[context.i];
		
		if (!token) {
			throw new CompileError("unexpected end of file").indicator(context.tokens[context.i-1].location, "last token here");
		}
		
		return token;
	}
	
	if (next().type == TokenType.separator && next().text == ")") {
		forward();
		return AST;
	}
	
	while (context.i < context.tokens.length) {
		const name = forward();
		if (name.type != TokenType.word) {
			throw new CompileError("expected name in function arguments").indicator(name.location, "here");
		}
		
		const colon = forward();
		if (colon.type != TokenType.separator || colon.text != ":") {
			throw new CompileError("expected colon in function arguments").indicator(colon.location, "here");
		}
		
		const type = parse(context, ParserMode.single, null);
		
		AST.push({
			kind: "argument",
			location: name.location,
			name: name.text,
			type: type,
		});
		
		const end = forward();
		
		if (end.type == TokenType.separator && end.text == ")") {
			return AST;
		} else if (end.type == TokenType.separator && end.text == ",") {
			continue;
		} else {
			throw new CompileError("expected a comma in function arguments").indicator(end.location, "here");
		}
	}
	
	return AST;
}

export function parse(context: ParserContext, mode: ParserMode, endAt: ")" | "}" | "]" | null): ASTnode[] {
	let AST: ASTnode[] = [];
	
	function forward(): Token {
		const token = context.tokens[context.i];
		
		if (!token) {
			throw new CompileError("unexpected end of file").indicator(context.tokens[context.i-1].location, "last token here");
		}
		
		context.i++
		return token;
	}
	
	function next(): Token {
		const token = context.tokens[context.i];
		
		if (!token) {
			throw new CompileError("unexpected end of file").indicator(context.tokens[context.i-1].location, "last token here");
		}
		
		return token;
	}
	
	while (context.i < context.tokens.length) {
		const token = forward();
		
		let needsSemicolon = true;
		
		switch (token.type) {
			case TokenType.comment: {
				continue;
			}
			
			case TokenType.number: {
				AST.push({
					kind: "number",
					location: token.location,
					value: Number(token.text),
				});
				break;
			}
			
			case TokenType.string: {
				AST.push({
					kind: "string",
					location: token.location,
					value: token.text,
				});
				break;
			}
			
			case TokenType.word: {
				if (token.text == "const") {
					const name = forward();
					if (name.type != TokenType.word) {
						throw new CompileError("expected name").indicator(name.location, "here");
					}
					
					const equals = forward();
					if (equals.type != TokenType.operator || equals.text != "=") {
						throw new CompileError("expected equals").indicator(equals.location, "here");
					}
					
					const value = parse(context, ParserMode.single, null);
					
					AST.push({
						kind: "definition",
						location: token.location,
						mutable: false,
						name: name.text,
						value: value,
					});
				}
				
				else if (token.text == "fn") {
					const openingParentheses = forward();
					if (openingParentheses.type != TokenType.separator || openingParentheses.text != "(") {
						throw new CompileError("expected openingParentheses").indicator(openingParentheses.location, "here");
					}
					
					const functionArguments = parseFunctionArguments(context);
					
					let returnType: ASTnode[] | null = null;
					
					const colonOrBracket = next();
					if (colonOrBracket.type == TokenType.separator && colonOrBracket.text == ":") {
						forward();
						
						returnType = parse(context, ParserMode.single, null);
					}
					
					const openingBracket = forward();
					if (openingBracket.type != TokenType.separator || openingBracket.text != "{") {
						throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
					}
					
					const codeBlock = parse(context, ParserMode.normal, "}");
					
					AST.push({
						kind: "function",
						location: token.location,
						functionArguments: functionArguments,
						returnType: returnType,
						codeBlock: codeBlock,
					});
				}
				
				else if (token.text == "ret") {
					const value = parse(context, ParserMode.single, null);
					
					AST.push({
						kind: "return",
						location: token.location,
						value: value,
					});
				}
				
				else {
					AST.push({
						kind: "identifier",
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
				throw new CompileError("unexpected separator").indicator(token.location, "here");
				break;
			}
			
			case TokenType.builtinIndicator: {
				const name = forward();
				if (name.type != TokenType.word) {
					throw new CompileError("expected name").indicator(name.location, "here");
				}
				
				const openingParentheses = forward();
				if (openingParentheses.type != TokenType.separator || openingParentheses.text != "(") {
					throw new CompileError("expected openingParentheses").indicator(openingParentheses.location, "here");
				}
				
				const callArguments = parse(context, ParserMode.comma, ")");
				
				AST.push({
					kind: "builtinCall",
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
		
		if (next().type == TokenType.separator && next().text == "(") {
			const left = AST.pop();
			if (left != undefined) {
				forward();
				const callArguments = parse(context, ParserMode.comma, ")");
				
				AST.push({
					kind: "call",
					location: next().location,
					left: [left],
					callArguments: callArguments,
				});
			} else {
				utilities.unreachable();
			}
		}
		
		if (mode != ParserMode.comma && needsSemicolon) {
			if (!next()) {
				throw new CompileError(`expected a semicolon but the file ended`).indicator(context.tokens[context.i-1].location, "here");
			}
			const semicolon = forward();
			if (semicolon.type != TokenType.separator || semicolon.text != ";") {
				throw new CompileError(`expected a semicolon but got '${semicolon.text}'`).indicator(semicolon.location, "here");
			}
		}
		
		if (endAt != null) {
			const nextToken = next();
			
			if (nextToken.type == TokenType.separator) {
				if (nextToken.text == ")" || nextToken.text == "}" || nextToken.text == "]") {
					if (endAt == nextToken.text) {
						context.i++;
						return AST;
					} else {
						throw new CompileError("unexpected separator")
							.indicator(nextToken.location, `expected '${endAt} but got '${nextToken.text}'`);
					}
				}
			}
		}
		
		if (mode == ParserMode.comma) {
			const comma = forward();
			if (comma.type != TokenType.separator || comma.text != ",") {
				throw new CompileError("expected a comma").indicator(comma.location, "here");
			}
		}
	}
	
	return AST;
}
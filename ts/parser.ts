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
	singleNoContinue,
	comma,
}

function getOperatorPrecedence(operatorText: string): number {
	if (operatorText == "=") {
		return 1;
	}
	
	else if (operatorText == "||") {
		return 2;
	}
	
	else if (operatorText == "&&") {
		return 3;
	}
	
	else if (
		operatorText == "==" ||
		operatorText == "!=" ||
		operatorText == ">" ||
		operatorText == "<"
	) {
		return 4;
	}
	
	else if (
		operatorText == "+" ||
		operatorText == "-"
	) {
		return 5;
	}
	
	else if (
		operatorText == "*" ||
		operatorText == "/" ||
		operatorText == "%"
	) {
		return 6;
	}
	
	else if (operatorText == "as") {
		return 7;
	}
	
	else if (operatorText == ".") {
		return 8;
	}
	
	else {
		utilities.unreachable();
		return -1;
	}
}

function parseOperators(context: ParserContext, left: ASTnode, lastPrecedence: number): ASTnode {
	if (!context.tokens[context.i]) {
		return left;
	}
	
	const nextOperator = context.tokens[context.i];
	
	if (nextOperator.type == TokenType.operator) {
		const nextPrecedence = getOperatorPrecedence(nextOperator.text);
		
		if (nextPrecedence > lastPrecedence) {
			let right: ASTnode;
			context.i++;
			let mode: ParserMode;
			if (nextOperator.text == ".") {
				mode = ParserMode.singleNoContinue;
			} else {
				mode = ParserMode.single;
			}
			right = parseOperators(context, parse(context, mode, null)[0], nextPrecedence);
			
			return {
				kind: "operator",
				location: nextOperator.location,
				operatorText: nextOperator.text,
				left: [left],
				right: [right],
			}	
		}
	}
	
	return left;
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
				if (token.text == "true" || token.text == "false") {
					AST.push({
						kind: "bool",
						location: token.location,
						value: token.text == "true",
					});
				} else if (token.text == "const" || token.text == "var") {
					const name = forward();
					if (name.type != TokenType.word) {
						throw new CompileError("expected name").indicator(name.location, "here");
					}
					
					const equals = forward();
					if (equals.type != TokenType.operator || equals.text != "=") {
						throw new CompileError("expected equals").indicator(equals.location, "here");
					}
					
					const value = parse(context, ParserMode.single, null)[0];
					
					if (!value) {
						throw new CompileError("empty definition").indicator(name.location, "here");
					}
					
					AST.push({
						kind: "definition",
						location: token.location,
						mutable: token.text == "var",
						name: name.text,
						value: value,
					});
				}
				
				else if (token.text == "codeGenerate") {
					const openingBracket = forward();
					if (openingBracket.type != TokenType.separator || openingBracket.text != "{") {
						throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
					}
					
					const codeBlock = parse(context, ParserMode.normal, "}");
					
					AST.push({
						kind: "codeGenerate",
						location: token.location,
						codeBlock: codeBlock,
					});
					
					needsSemicolon = false;
				}
				
				else if (token.text == "while" || token.text == "if") {
					const openingParentheses = forward();
					if (openingParentheses.type != TokenType.separator || openingParentheses.text != "(") {
						throw new CompileError("expected openingParentheses").indicator(openingParentheses.location, "here");
					}
					
					const condition = parse(context, ParserMode.single, null);
					
					const closingParentheses = forward();
					if (closingParentheses.type != TokenType.separator || closingParentheses.text != ")") {
						throw new CompileError("expected closingParentheses").indicator(closingParentheses.location, "here");
					}
					
					const openingBracket = forward();
					if (openingBracket.type != TokenType.separator || openingBracket.text != "{") {
						throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
					}
					
					const codeBlock = parse(context, ParserMode.normal, "}");
					
					if (token.text == "while") {
						AST.push({
							kind: "while",
							location: token.location,
							condition: condition,
							codeBlock: codeBlock,
						});	
					} else {
						AST.push({
							kind: "if",
							location: token.location,
							condition: condition,
							codeBlock: codeBlock,
						});	
					}
					
					needsSemicolon = false;
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
		
		if (mode == ParserMode.singleNoContinue) {
			return AST;
		}
		
		if (context.tokens[context.i] && next().type == TokenType.operator) {
			const left = AST.pop();
			if (left != undefined) {
				AST.push(parseOperators(context, left, 0));
			} else {
				utilities.unreachable();
			}
		}
		
		if (context.tokens[context.i] && next().type == TokenType.separator && next().text == "(") {
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
		
		if (mode == ParserMode.single) {
			return AST;
		}
		
		if (mode != ParserMode.comma && needsSemicolon) {
			if (!context.tokens[context.i]) {
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
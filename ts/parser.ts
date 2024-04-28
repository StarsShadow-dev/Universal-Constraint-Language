import {
	TokenType,
	Token,
	
	ASTnode,
	SourceLocation,
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
	singleNoOperatorContinue,
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
		throw utilities.unreachable();
	}
}

function forward(context: ParserContext): Token {
	const token = context.tokens[context.i];
	
	if (!token) {
		throw new CompileError("unexpected end of file").indicator(context.tokens[context.i-1].location, "last token here");
	}
	
	context.i++
	return token;
}

function next(context: ParserContext): Token {
	const token = context.tokens[context.i];
	
	if (!token) {
		throw new CompileError("unexpected end of file").indicator(context.tokens[context.i-1].location, "last token here");
	}
	
	return token;
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

function parseType(context: ParserContext): ASTnode & { kind: "typeUse" } | null {
	let type: ASTnode & { kind: "typeUse" } | null = null;
	
	const colon = next(context);
	if (colon.type == TokenType.separator && colon.text == ":") {
		forward(context);
		
		const node = parse(context, ParserMode.singleNoOperatorContinue, null)[0];
		type = {
			kind: "typeUse",
			location: node.location,
			value: node,
		}
	}
	
	return type;
}

function parseFunctionArguments(context: ParserContext): ASTnode[] {
	let AST: ASTnode[] = [];
	
	if (next(context).type == TokenType.separator && next(context).text == ")") {
		forward(context);
		return AST;
	}
	
	while (context.i < context.tokens.length) {
		const name = forward(context);
		if (name.type != TokenType.word) {
			throw new CompileError("expected name in function arguments").indicator(name.location, "here");
		}
		
		const type = parseType(context);
		
		if (type) {
			AST.push({
				kind: "argument",
				location: name.location,
				name: name.text,
				type: type,
			});	
		} else {
			throw new CompileError("function argument without a type").indicator(name.location, "here");
		}
		
		const end = forward(context);
		
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

function parseFunction(context: ParserContext, AST: ASTnode[], location: SourceLocation, forceInline: boolean) {
	const openingParentheses = forward(context);
	if (openingParentheses.type != TokenType.separator || openingParentheses.text != "(") {
		throw new CompileError("expected openingParentheses").indicator(openingParentheses.location, "here");
	}
	
	const functionArguments = parseFunctionArguments(context);
	
	const returnType = parseType(context);
	
	const openingBracket = forward(context);
	if (openingBracket.type != TokenType.separator || openingBracket.text != "{") {
		throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
	}
	
	const codeBlock = parse(context, ParserMode.normal, "}");
	
	AST.push({
		kind: "function",
		location: location,
		forceInline: forceInline,
		functionArguments: functionArguments,
		returnType: returnType,
		codeBlock: codeBlock,
	});
}

export function parse(context: ParserContext, mode: ParserMode, endAt: ")" | "}" | "]" | null): ASTnode[] {
	let AST: ASTnode[] = [];
	
	while (context.i < context.tokens.length) {
		const token = forward(context);
		
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
				}
				
				else if (token.text == "const" || token.text == "var" || token.text == "property") {
					const name = forward(context);
					if (name.type != TokenType.word) {
						throw new CompileError("expected name").indicator(name.location, "here");
					}
					
					const type = parseType(context);
					
					let value: ASTnode | null = null;
					
					const equals = next(context);
					if (equals.type == TokenType.operator || equals.text == "=") {
						forward(context);
						
						value = parse(context, ParserMode.single, null)[0];
						if (!value) {
							throw new CompileError("empty definition").indicator(name.location, "here");
						}
					}
					
					AST.push({
						kind: "definition",
						location: token.location,
						mutable: token.text != "const",
						isAproperty: token.text == "property",
						name: name.text,
						type: type,
						value: value,
					});
				}
				
				else if (token.text == "codeGenerate") {
					const openingBracket = forward(context);
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
					const openingParentheses = forward(context);
					if (openingParentheses.type != TokenType.separator || openingParentheses.text != "(") {
						throw new CompileError("expected openingParentheses").indicator(openingParentheses.location, "here");
					}
					
					const condition = parse(context, ParserMode.single, null);
					
					const closingParentheses = forward(context);
					if (closingParentheses.type != TokenType.separator || closingParentheses.text != ")") {
						throw new CompileError("expected closingParentheses").indicator(closingParentheses.location, "here");
					}
					
					const openingBracket = forward(context);
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
					} else if (token.text == "if") {
						let falseCodeBlock: ASTnode[] | null = null;
						
						if (context.tokens[context.i]) {
							const elseToken = next(context);
							if (elseToken.type == TokenType.word && elseToken.text == "else") {
								forward(context);
								const elseOpeningBracket = forward(context);
								if (elseOpeningBracket.type != TokenType.separator || elseOpeningBracket.text != "{") {
									throw new CompileError("expected openingBracket for else code block")
										.indicator(elseOpeningBracket.location, "here");
								}
								falseCodeBlock = parse(context, ParserMode.normal, "}");
							}
						}
						
						AST.push({
							kind: "if",
							location: token.location,
							condition: condition,
							trueCodeBlock: codeBlock,
							falseCodeBlock: falseCodeBlock,
						});	
					}
					
					needsSemicolon = false;
				}
				
				else if (token.text == "inline") {
					if (next(context).text == "fn") {
						const fnToken = forward(context);
						parseFunction(context, AST, fnToken.location, true);
					}
				}
				
				else if (token.text == "struct") {
					const templateType = parseType(context);
					
					const openingBracket = forward(context);
					if (openingBracket.type != TokenType.separator || openingBracket.text != "{") {
						throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
					}
					
					const codeBlock = parse(context, ParserMode.normal, "}");
					
					AST.push({
						kind: "struct",
						location: token.location,
						templateType: templateType,
						codeBlock: codeBlock,
					});
					
					needsSemicolon = false;
				}
				
				else if (token.text == "fn") {
					parseFunction(context, AST, token.location, false);
				}
				
				else if (token.text == "ret") {
					const semicolon = next(context);
					if (semicolon.type == TokenType.separator && semicolon.text == ";") {
						AST.push({
							kind: "return",
							location: token.location,
							value: null,
						});
					} else {
						const value = parse(context, ParserMode.single, null)[0];
						
						AST.push({
							kind: "return",
							location: token.location,
							value: value,
						});
					}
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
			case TokenType.operator: {
				const left = AST.pop();
				if (left != undefined) {
					context.i--;
					AST.push(parseOperators(context, left, 0));
				} else {
					utilities.unreachable();
				}
				break;
			}
			
			case TokenType.builtinIndicator: {
				const name = forward(context);
				if (name.type != TokenType.word) {
					throw new CompileError("expected name").indicator(name.location, "here");
				}
				
				const openingParentheses = forward(context);
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
			
			case TokenType.singleQuote: {
				const node = parse(context, ParserMode.singleNoOperatorContinue, null)[0];
				AST.push({
					kind: "comptime",
					location: token.location,
					value: node,
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
		
		if (context.tokens[context.i] && next(context).type == TokenType.separator && next(context).text == "(") {
			const left = AST.pop();
			if (left != undefined) {
				forward(context);
				const callArguments = parse(context, ParserMode.comma, ")");
				
				AST.push({
					kind: "call",
					location: next(context).location,
					left: [left],
					callArguments: callArguments,
				});
			} else {
				utilities.unreachable();
			}
		}
		
		if (mode == ParserMode.singleNoOperatorContinue) {
			return AST;
		}
		
		if (context.tokens[context.i] && next(context).type == TokenType.operator) {
			continue;
		}
		
		if (mode == ParserMode.single) {
			return AST;
		}
		
		if (mode != ParserMode.comma && needsSemicolon) {
			if (!context.tokens[context.i]) {
				throw new CompileError(`expected a semicolon but the file ended`).indicator(context.tokens[context.i-1].location, "here");
			}
			const semicolon = forward(context);
			if (semicolon.type != TokenType.separator || semicolon.text != ";") {
				throw new CompileError(`expected a semicolon but got '${semicolon.text}'`).indicator(semicolon.location, "here");
			}
		}
		
		if (endAt != null) {
			const nextToken = next(context);
			
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
			const comma = forward(context);
			if (comma.type != TokenType.separator || comma.text != ",") {
				throw new CompileError("expected a comma").indicator(comma.location, "here");
			}
		}
	}
	
	return AST;
}
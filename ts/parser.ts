import utilities from "./utilities";
import {
	TokenType,
	Token,
	ASTnode,
	SourceLocation,
} from "./types";
import { CompileError } from "./report";

export type ParserContext = {
	tokens: Token[],
	i: number,
}

export enum ParserMode {
	normal,
	single,
	singleNoContinue,
	singleNoOperatorContinue,
	singleNoEqualsOperatorContinue,
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
			let mode;
			if (nextOperator.text == "=") {
				mode = ParserMode.single;
			} else if (nextOperator.text == ".") {
				mode = ParserMode.singleNoContinue;
			} else {
				mode = ParserMode.singleNoOperatorContinue;
			}
			right = parseOperators(context, parse(context, mode, null)[0], nextPrecedence);
			
			if (nextOperator.text == "=") {
				if (left.kind != "identifier") {
					throw new CompileError("left side of assignment must be an identifier")
						.indicator(left.location, "left side here");
				}
				return {
					kind: "definition",
					location: nextOperator.location,
					name: left.name,
					value: right,
				}
			} else {
				return {
					kind: "operator",
					location: nextOperator.location,
					operatorText: nextOperator.text,
					left: [left],
					right: [right],
				}
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
		
		const node = parse(context, ParserMode.singleNoEqualsOperatorContinue, null)[0];
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
		let comptime = false;
		if (
			next(context).type == TokenType.word &&
			next(context).text == "comptime"
		) {
			comptime = true;
			forward(context);
		}
		
		const name = forward(context);
		if (name.type != TokenType.word) {
			throw new CompileError("expected name in function arguments").indicator(name.location, "here");
		}
		
		const type = parseType(context);
		
		if (type) {
			AST.push({
				kind: "argument",
				location: name.location,
				comptime: comptime,
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
	
	let comptimeReturn = false;
	if (
		next(context).type == TokenType.word &&
		next(context).text == "comptime"
	) {
		comptimeReturn = true;
		forward(context);
	}
	
	const returnType = parseType(context);
	if (!returnType) {
		throw new CompileError("functions must have a return type").indicator(location, "here");
	}
	
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
		comptimeReturn: comptimeReturn,
		codeBlock: codeBlock,
	});
}

export function parse(context: ParserContext, mode: ParserMode, endAt: ")" | "}" | "]" | null): ASTnode[] {
	let AST: ASTnode[] = [];
	
	while (context.i < context.tokens.length) {
		let token = forward(context);
		
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
				
				else if (token.text == "field") {
					const name = forward(context);
					if (name.type != TokenType.word) {
						throw new CompileError("expected name").indicator(name.location, "here");
					}
					
					const type = parseType(context);
					if (!type) {
						throw new CompileError("a field must have a type").indicator(name.location, "here");
					}
					
					AST.push({
						kind: "field",
						location: token.location,
						name: name.text,
						type: type,
					});
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
				}
				
				else if (token.text == "inline") {
					if (next(context).text == "fn") {
						const fnToken = forward(context);
						parseFunction(context, AST, fnToken.location, true);
					}
				}
				
				else if (token.text == "struct") {
					const openingParentheses = forward(context);
					if (openingParentheses.type != TokenType.separator || openingParentheses.text != "(") {
						throw new CompileError("expected openingParentheses").indicator(openingParentheses.location, "here");
					}
					const fields = parseFunctionArguments(context);
					
					const openingBracket = forward(context);
					if (openingBracket.type != TokenType.separator || openingBracket.text != "{") {
						throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
					}
					const codeBlock = parse(context, ParserMode.normal, "}");
					
					AST.push({
						kind: "struct",
						location: token.location,
						fields: fields,
						codeBlock: codeBlock,
					});
				}
				
				else if (token.text == "enum") {
					const openingBracket = forward(context);
					if (openingBracket.type != TokenType.separator || openingBracket.text != "{") {
						throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
					}
					const codeBlock = parse(context, ParserMode.normal, "}");
					
					AST.push({
						kind: "enum",
						location: token.location,
						codeBlock: codeBlock,
					});
				}
				
				else if (token.text == "match") {
					const expression = parse(context, ParserMode.singleNoContinue, "}")[0];
					
					const openingBracket = forward(context);
					if (openingBracket.type != TokenType.separator || openingBracket.text != "{") {
						throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
					}
					const codeBlock = parse(context, ParserMode.normal, "}");
					
					AST.push({
						kind: "match",
						location: token.location,
						expression: expression,
						codeBlock: codeBlock,
					});
				}
				
				else if (token.text == "fn") {
					parseFunction(context, AST, token.location, false);
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
				if (token.text == "[") {
					const elements = parse(context, ParserMode.comma, "]")
					
					AST.push({
						kind: "list",
						location: token.location,
						elements: elements,
					});
				} else {
					throw new CompileError("unexpected separator").indicator(token.location, "here");
				}
				break;
			}
			case TokenType.operator: {
				const left = AST[AST.length-1];
				if (left == undefined) {
					utilities.TODO();
				} else {
					if (token.text == "->") {
						const identifier = AST.pop();
						if (!identifier || (identifier.kind != "identifier" && identifier.kind != "call")) {
							throw utilities.TODO();
						}
						
						const openingBracket = forward(context);
						if (openingBracket.type != TokenType.separator || openingBracket.text != "{") {
							throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
						}
						const codeBlock = parse(context, ParserMode.normal, "}");
						
						let name = "";
						let types: ASTnode[] = [];
						
						if (identifier.kind == "identifier") {
							name = identifier.name;
						} else {
							if (identifier.left.kind != "identifier") {
								throw utilities.TODO();
							}
							name = identifier.left.name;
							types = identifier.callArguments;
						}
						
						AST.push({
							kind: "matchCase",
							location: identifier.location,
							name: name,
							types: types,
							codeBlock: codeBlock,
						});
					} else {
						context.i--;
						AST.pop();
						AST.push(parseOperators(context, left, 0));
					}
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
				utilities.unreachable();
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
				const location = forward(context).location;
				const callArguments = parse(context, ParserMode.comma, ")");
				
				AST.push({
					kind: "call",
					location: location,
					left: left,
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
			if (mode == ParserMode.singleNoEqualsOperatorContinue && next(context).text == "=") {
				return AST;
			}
			continue;
		}
		
		if (mode == ParserMode.singleNoEqualsOperatorContinue) {
			return AST;
		}
		
		if (context.tokens[context.i] && next(context).type == TokenType.separator && next(context).text == "{") {
			const left = AST.pop();
			if (left) {
				forward(context);
				const codeBlock = parse(context, ParserMode.normal, "}");
				
				AST.push({
					kind: "newInstance",
					location: next(context).location,
					template: {
						kind: "typeUse",
						location: left.location,
						value: left,
					},
					codeBlock: codeBlock,
				});
			} else {
				utilities.unreachable();
			}
		}
		
		if (mode == ParserMode.single) {
			return AST;
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
import utilities from "./utilities";
import {
	TokenKind,
	Token,
	OpCode,
	SourceLocation,
	OpCode_argument,
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
	singleNoNewInstanceContinue,
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

function more(context: ParserContext): boolean {
	return context.tokens[context.i] != undefined;
}

function forward(context: ParserContext): Token {
	const token = context.tokens[context.i];
	
	if (!token) {
		const endToken = context.tokens[context.i-1];
		return {
			type: TokenKind.endOfFile,
			text: "[end of file]",
			location: endToken.location,
		};
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

function parseOperators(context: ParserContext, left: OpCode, lastPrecedence: number): OpCode {
	if (!context.tokens[context.i]) {
		return left;
	}
	
	const nextOperator = context.tokens[context.i];
	
	if (nextOperator.type == TokenKind.operator) {
		const nextPrecedence = getOperatorPrecedence(nextOperator.text);
		
		if (nextPrecedence > lastPrecedence) {
			let right: OpCode;
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
					kind: "alias",
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

function parseType(context: ParserContext): OpCode | null {
	let type: OpCode | null = null;
	
	const colon = next(context);
	if (colon.type == TokenKind.separator && colon.text == ":") {
		forward(context);
		
		type = parse(context, ParserMode.singleNoEqualsOperatorContinue, null)[0];
	}
	
	return type;
}

function parseFunctionArguments(context: ParserContext): OpCode_argument[] {
	let AST: OpCode_argument[] = [];
	
	if (next(context).type == TokenKind.separator && next(context).text == ")") {
		forward(context);
		return AST;
	}
	
	while (context.i < context.tokens.length) {
		let comptime = false;
		if (
			next(context).type == TokenKind.word &&
			next(context).text == "comptime"
		) {
			comptime = true;
			forward(context);
		}
		
		const name = forward(context);
		if (name.type != TokenKind.word) {
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
		
		if (end.type == TokenKind.separator && end.text == ")") {
			return AST;
		} else if (end.type == TokenKind.separator && end.text == ",") {
			continue;
		} else {
			throw new CompileError("expected a comma in function arguments").indicator(end.location, "here");
		}
	}
	
	return AST;
}

function parseFunction(context: ParserContext, AST: OpCode[], location: SourceLocation, forceInline: boolean) {
	const openingParentheses = forward(context);
	if (openingParentheses.type != TokenKind.separator || openingParentheses.text != "(") {
		throw new CompileError("expected openingParentheses").indicator(openingParentheses.location, "here");
	}
	
	const functionArguments = parseFunctionArguments(context);
	
	let comptimeReturn = false;
	if (
		next(context).type == TokenKind.word &&
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
	if (openingBracket.type != TokenKind.separator || openingBracket.text != "{") {
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

export function parse(context: ParserContext, mode: ParserMode, endAt: ")" | "}" | "]" | null): OpCode[] {
	let OpCodes: OpCode[] = [];
	
	function earlyReturn() {
		if (endAt && more(context) && next(context).type == TokenKind.separator && next(context).text != ",") {
			if (next(context).text == endAt) {
				forward(context);
			} else {
				utilities.TODO();
			}
		}
	}
	
	while (context.i < context.tokens.length) {
		let token = forward(context);
		
		switch (token.type) {
			case TokenKind.comment: {
				continue;
			}
			
			case TokenKind.number: {
				OpCodes.push({
					kind: "number",
					location: token.location,
					value: Number(token.text),
				});
				break;
			}
			
			case TokenKind.string: {
				OpCodes.push({
					kind: "string",
					location: token.location,
					value: token.text,
				});
				break;
			}
			
			case TokenKind.word: {
				if (token.text == "true" || token.text == "false") {
					OpCodes.push({
						kind: "bool",
						location: token.location,
						value: token.text == "true",
					});
				}
				
				else if (token.text == "field") {
					const name = forward(context);
					if (name.type != TokenKind.word) {
						throw new CompileError("expected name").indicator(name.location, "here");
					}
					
					const type = parseType(context);
					if (!type) {
						throw new CompileError("a field must have a type").indicator(name.location, "here");
					}
					
					OpCodes.push({
						kind: "field",
						location: token.location,
						name: name.text,
						type: type,
					});
				}
				
				else if (token.text == "while" || token.text == "if") {
					const openingParentheses = forward(context);
					if (openingParentheses.type != TokenKind.separator || openingParentheses.text != "(") {
						throw new CompileError("expected openingParentheses").indicator(openingParentheses.location, "here");
					}
					
					const condition = parse(context, ParserMode.single, ")")[0];
					
					const openingBracket = forward(context);
					if (openingBracket.type != TokenKind.separator || openingBracket.text != "{") {
						throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
					}
					
					const codeBlock = parse(context, ParserMode.normal, "}");
					
					if (token.text == "while") {
						OpCodes.push({
							kind: "while",
							location: token.location,
							condition: condition,
							codeBlock: codeBlock,
						});	
					} else if (token.text == "if") {
						let falseCodeBlock: OpCode[] | null = null;
						
						const elseToken = forward(context);
						if (elseToken.type != TokenKind.word || elseToken.text != "else") {
							throw new CompileError("expected 'else'")
								.indicator(elseToken.location, "here")
								.indicator(token.location, "for this if expression");
						}
						
						const elseOpeningBracket = forward(context);
						if (elseOpeningBracket.type != TokenKind.separator || elseOpeningBracket.text != "{") {
							throw new CompileError("expected openingBracket for else code block")
								.indicator(elseOpeningBracket.location, "here");
						}
						falseCodeBlock = parse(context, ParserMode.normal, "}");
						
						OpCodes.push({
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
						parseFunction(context, OpCodes, fnToken.location, true);
					}
				}
				
				else if (token.text == "struct") {
					const openingParentheses = forward(context);
					if (openingParentheses.type != TokenKind.separator || openingParentheses.text != "(") {
						throw new CompileError("expected openingParentheses").indicator(openingParentheses.location, "here");
					}
					const fields = parseFunctionArguments(context);
					
					const openingBracket = forward(context);
					if (openingBracket.type != TokenKind.separator || openingBracket.text != "{") {
						throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
					}
					const codeBlock = parse(context, ParserMode.normal, "}");
					
					OpCodes.push({
						kind: "struct",
						location: token.location,
						id: `${JSON.stringify(token.location)}`,
						fields: fields,
						codeBlock: codeBlock,
					});
				}
				
				else if (token.text == "enum") {
					const openingBracket = forward(context);
					if (openingBracket.type != TokenKind.separator || openingBracket.text != "{") {
						throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
					}
					const codeBlock = parse(context, ParserMode.normal, "}");
					
					OpCodes.push({
						kind: "enum",
						location: token.location,
						id: `${JSON.stringify(token.location)}`,
						codeBlock: codeBlock,
					});
				}
				
				else if (token.text == "match") {
					const expression = parse(context, ParserMode.singleNoNewInstanceContinue, null)[0];
					
					const openingBracket = forward(context);
					if (openingBracket.type != TokenKind.separator || openingBracket.text != "{") {
						throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
					}
					const codeBlock = parse(context, ParserMode.normal, "}");
					
					OpCodes.push({
						kind: "match",
						location: token.location,
						expression: expression,
						codeBlock: codeBlock,
					});
				}
				
				else if (token.text == "fn") {
					parseFunction(context, OpCodes, token.location, false);
				}
				
				else {
					OpCodes.push({
						kind: "identifier",
						location: token.location,
						name: token.text,
					});
				}
				break;
			}
			
			case TokenKind.separator: {
				if (token.text == endAt) {
					return OpCodes;
				}
				if (token.text == "[") {
					const elements = parse(context, ParserMode.comma, "]")
					
					OpCodes.push({
						kind: "list",
						location: token.location,
						elements: elements,
					});
				} else {
					throw new CompileError("unexpected separator").indicator(token.location, "here");
				}
				break;
			}
			case TokenKind.operator: {
				const left = OpCodes[OpCodes.length-1];
				if (left == undefined) {
					utilities.TODO();
				} else {
					if (token.text == "->") {
						const identifier = OpCodes.pop();
						if (!identifier || (identifier.kind != "identifier" && identifier.kind != "call")) {
							throw utilities.TODO();
						}
						
						const openingBracket = forward(context);
						if (openingBracket.type != TokenKind.separator || openingBracket.text != "{") {
							throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
						}
						const codeBlock = parse(context, ParserMode.normal, "}");
						
						let name = "";
						let types: OpCode[] = [];
						
						if (identifier.kind == "identifier") {
							name = identifier.name;
						} else {
							if (identifier.left.kind != "identifier") {
								throw utilities.TODO();
							}
							name = identifier.left.name;
							types = identifier.callArguments;
						}
						
						OpCodes.push({
							kind: "matchCase",
							location: identifier.location,
							name: name,
							types: types,
							codeBlock: codeBlock,
						});
					} else {
						context.i--;
						OpCodes.pop();
						OpCodes.push(parseOperators(context, left, 0));
					}
				}
				break;
			}
			
			case TokenKind.builtinIndicator: {
				const name = forward(context);
				if (name.type != TokenKind.word) {
					throw new CompileError("expected name").indicator(name.location, "here");
				}
				
				const openingParentheses = forward(context);
				if (openingParentheses.type != TokenKind.separator || openingParentheses.text != "(") {
					throw new CompileError("expected openingParentheses").indicator(openingParentheses.location, "here");
				}
				
				const callArguments = parse(context, ParserMode.comma, ")");
				
				OpCodes.push({
					kind: "builtinCall",
					location: name.location,
					name: name.text,
					callArguments: callArguments,
				});
				break;
			}
			
			case TokenKind.singleQuote: {
				utilities.unreachable();
				break;
			}
		
			default: {
				utilities.unreachable();
				break;
			}
		}
		
		if (mode == ParserMode.singleNoContinue) {
			earlyReturn();
			return OpCodes;
		}
		
		if (context.tokens[context.i] && next(context).type == TokenKind.separator && next(context).text == "(") {
			const left = OpCodes.pop();
			if (left != undefined) {
				const location = forward(context).location;
				const callArguments = parse(context, ParserMode.comma, ")");
				
				OpCodes.push({
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
			earlyReturn();
			return OpCodes;
		}
		
		if (context.tokens[context.i] && next(context).type == TokenKind.operator) {
			if (mode == ParserMode.singleNoEqualsOperatorContinue && next(context).text == "=") {
				earlyReturn();
				return OpCodes;
			}
			continue;
		}
		
		if (mode == ParserMode.singleNoEqualsOperatorContinue) {
			earlyReturn();
			return OpCodes;
		}
		
		if (mode == ParserMode.singleNoNewInstanceContinue) {
			earlyReturn();
			return OpCodes;
		}
		
		if (context.tokens[context.i] && next(context).type == TokenKind.separator && next(context).text == "{") {
			const left = OpCodes.pop();
			if (left) {
				forward(context);
				const codeBlock = parse(context, ParserMode.normal, "}");
				
				OpCodes.push({
					kind: "newInstance",
					location: left.location,
					template: left,
					codeBlock: codeBlock,
				});
			} else {
				utilities.unreachable();
			}
		}
		
		if (mode == ParserMode.single) {
			earlyReturn();
			return OpCodes;
		}
		
		if (endAt != null) {
			const nextToken = next(context);
			
			if (nextToken.type == TokenKind.separator) {
				if (nextToken.text == ")" || nextToken.text == "}" || nextToken.text == "]") {
					if (endAt == nextToken.text) {
						context.i++;
						return OpCodes;
					} else {
						throw new CompileError("unexpected separator")
							.indicator(nextToken.location, `expected '${endAt} but got '${nextToken.text}'`);
					}
				}
			}
		}
		
		if (mode == ParserMode.comma) {
			const comma = forward(context);
			if (comma.type != TokenKind.separator || comma.text != ",") {
				throw new CompileError("expected a comma").indicator(comma.location, "here");
			}
		}
	}
	
	return OpCodes;
}
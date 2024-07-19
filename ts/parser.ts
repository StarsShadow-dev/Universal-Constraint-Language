import utilities from "./utilities";
import { SourceLocation } from "./types";
import { CompileError } from "./report";
import { Token, TokenKind } from "./lexer";

type genericASTnode = {
	location: SourceLocation,
};

export type ASTnode_builtinCall = genericASTnode & {
	kind: "builtinCall",
	name: string,
	callArguments: ASTnode[],
};

export type ASTnode_argument = genericASTnode & {
	kind: "argument",
	name: string,
	type: ASTnode,
};

export type genericASTnodeType = genericASTnode & {
	id: string,
};
export function ASTnode_isAtype(node: ASTnode): node is ASTnodeType {
	if (node.kind == "struct" || node.kind == "enum" || node.kind == "functionType") {
		return true;
	}
	
	return false;
}

export type ASTnode_error = genericASTnode & {
	kind: "error",
}

export type ASTnodeType =
genericASTnodeType & {
	kind: "struct",
	fields: ASTnode_argument[],
	data?: {
		kind: "number",
		min: number,
		max: number,
	},
} | genericASTnodeType & {
	kind: "enum",
	codeBlock: ASTnode[],
} | genericASTnodeType & {
	kind: "functionType",
	functionArguments: ASTnode[],
	returnType: ASTnode,
};

export type ASTnode_function = genericASTnode & {
	kind: "function",
	hasError: boolean,
	functionArguments: ASTnode_argument[],
	returnType: ASTnode,
	codeBlock: ASTnode[],
};

export type ASTnode_alias = genericASTnode & {
	kind: "alias",
	name: string,
	value: ASTnode,
};

export type ASTnode =
genericASTnode & {
	kind: "bool",
	value: boolean,
} | genericASTnode & {
	kind: "number",
	value: number,
} | genericASTnode & {
	kind: "string",
	value: string,
} | genericASTnode & {
	kind: "identifier",
	name: string,
} | genericASTnode & {
	kind: "list",
	elements: ASTnode[],
} | genericASTnode & {
	kind: "call",
	left: ASTnode,
	callArguments: ASTnode[],
} |
ASTnode_builtinCall |
genericASTnode & {
	kind: "operator",
	operatorText: string,
	left: ASTnode,
	right: ASTnode,
} | genericASTnode & {
	kind: "field",
	name: string,
	type: ASTnode,
} |
ASTnode_argument |
ASTnodeType |
ASTnode_function |
genericASTnode & {
	kind: "match",
	expression: ASTnode,
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "matchCase",
	name: string,
	types: ASTnode[],
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "while",
	condition: ASTnode,
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "if",
	condition: ASTnode,
	trueCodeBlock: ASTnode[],
	falseCodeBlock: ASTnode[],
} | genericASTnode & {
	kind: "codeBlock",
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "instance",
	template: ASTnode,
	codeBlock: ASTnode[],
} |
ASTnode_alias |
ASTnode_error;

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

function parseOperators(context: ParserContext, left: ASTnode, lastPrecedence: number): ASTnode {
	if (!context.tokens[context.i]) {
		return left;
	}
	
	const nextOperator = context.tokens[context.i];
	
	if (nextOperator.type == TokenKind.operator) {
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
					left: left,
					right: right,
				}
			}
		}
	}
	
	return left;
}

function parseType(context: ParserContext, separatingText: string): ASTnode | null {
	let type: ASTnode | null = null;
	
	const separator = next(context);
	if (separator.text == separatingText) {
		forward(context);
		
		type = parse(context, ParserMode.singleNoEqualsOperatorContinue, null)[0];
	}
	
	return type;
}

function parseFunctionArguments(context: ParserContext): ASTnode_argument[] {
	let AST: ASTnode_argument[] = [];
	
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
		
		const type = parseType(context, ":");
		
		if (!type) {
			throw new CompileError("function argument without a type").indicator(name.location, "here");
		}
		
		AST.push({
			kind: "argument",
			location: name.location,
			name: name.text,
			type: type,
		});	
		
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

function parseFunction(context: ParserContext, AST: ASTnode[], location: SourceLocation, forceInline: boolean) {
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
	
	const returnType = parseType(context, "->");
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
		hasError: false,
		functionArguments: functionArguments,
		returnType: returnType,
		codeBlock: codeBlock,
	});
}

export function parse(context: ParserContext, mode: ParserMode, endAt: ")" | "}" | "]" | null): ASTnode[] {
	let ASTnodes: ASTnode[] = [];
	
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
				ASTnodes.push({
					kind: "number",
					location: token.location,
					value: Number(token.text),
				});
				break;
			}
			
			case TokenKind.string: {
				ASTnodes.push({
					kind: "string",
					location: token.location,
					value: token.text,
				});
				break;
			}
			
			case TokenKind.word: {
				if (token.text == "true" || token.text == "false") {
					ASTnodes.push({
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
					
					const type = parseType(context, ":");
					if (!type) {
						throw new CompileError("a field must have a type").indicator(name.location, "here");
					}
					
					ASTnodes.push({
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
					if (!condition) {
						throw new CompileError("if expression is missing a condition").indicator(token.location, "here");
					}
					
					const openingBracket = forward(context);
					if (openingBracket.type != TokenKind.separator || openingBracket.text != "{") {
						throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
					}
					
					const codeBlock = parse(context, ParserMode.normal, "}");
					
					if (token.text == "while") {
						ASTnodes.push({
							kind: "while",
							location: token.location,
							condition: condition,
							codeBlock: codeBlock,
						});	
					} else if (token.text == "if") {
						let falseCodeBlock: ASTnode[] | null = null;
						
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
						
						ASTnodes.push({
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
						parseFunction(context, ASTnodes, fnToken.location, true);
					}
				}
				
				else if (token.text == "struct") {
					const openingParentheses = forward(context);
					if (openingParentheses.type != TokenKind.separator || openingParentheses.text != "(") {
						throw new CompileError("expected openingParentheses").indicator(openingParentheses.location, "here");
					}
					const fields = parseFunctionArguments(context);
					
					// const openingBracket = forward(context);
					// if (openingBracket.type != TokenKind.separator || openingBracket.text != "{") {
					// 	throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
					// }
					// const codeBlock = parse(context, ParserMode.normal, "}");
					
					ASTnodes.push({
						kind: "struct",
						location: token.location,
						id: JSON.stringify(token.location),
						fields: fields,
					});
				}
				
				else if (token.text == "enum") {
					const openingBracket = forward(context);
					if (openingBracket.type != TokenKind.separator || openingBracket.text != "{") {
						throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
					}
					const codeBlock = parse(context, ParserMode.normal, "}");
					for (let i = 0; i < codeBlock.length; i++) {
						const ASTnode = codeBlock[i];
						if (ASTnode.kind == "identifier") {
							codeBlock[i] = {
								kind: "alias",
								location: ASTnode.location,
								name: ASTnode.name,
								value: {
									kind: "struct",
									location: ASTnode.location,
									id: JSON.stringify(token.location),
									fields: [],
								},
							};
						}
					}
					
					ASTnodes.push({
						kind: "enum",
						location: token.location,
						id: JSON.stringify(token.location),
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
					
					ASTnodes.push({
						kind: "match",
						location: token.location,
						expression: expression,
						codeBlock: codeBlock,
					});
				}
				
				else if (token.text == "fn") {
					parseFunction(context, ASTnodes, token.location, false);
				}
				
				else {
					ASTnodes.push({
						kind: "identifier",
						location: token.location,
						name: token.text,
					});
				}
				break;
			}
			
			case TokenKind.separator: {
				if (token.text == endAt) {
					return ASTnodes;
				}
				if (token.text == "[") {
					const elements = parse(context, ParserMode.comma, "]");
					
					ASTnodes.push({
						kind: "list",
						location: token.location,
						elements: elements,
					});
				} else if (token.text == "(") {
					const left = ASTnodes.pop();
					if (left == undefined) {
						throw utilities.TODO();
					}
						
					const callArguments = parse(context, ParserMode.comma, ")");
					
					ASTnodes.push({
						kind: "call",
						location: token.location,
						left: left,
						callArguments: callArguments,
					});
				} else if (token.text == "\\") {
					const openingParentheses = forward(context);
					if (openingParentheses.type != TokenKind.separator || openingParentheses.text != "(") {
						throw new CompileError("expected openingParentheses").indicator(openingParentheses.location, "here");
					}
					
					const functionArguments = parse(context, ParserMode.comma, ")");
					const returnType = parseType(context, "->");
					if (!returnType) {
						throw new CompileError("function type must have a return type").indicator(token.location, "here");
					}
					
					ASTnodes.push({
						kind: "functionType",
						location: token.location,
						id: JSON.stringify(token.location),
						functionArguments: functionArguments,
						returnType: returnType,
					});
				} else {
					throw new CompileError("unexpected separator").indicator(token.location, "here");
				}
				break;
			}
			case TokenKind.operator: {
				const left = ASTnodes[ASTnodes.length-1];
				if (left == undefined) {
					utilities.TODO();
				} else {
					if (token.text == "->") {
						const identifier = ASTnodes.pop();
						if (!identifier || (identifier.kind != "identifier" && identifier.kind != "call")) {
							throw utilities.TODO();
						}
						
						const openingBracket = forward(context);
						if (openingBracket.type != TokenKind.separator || openingBracket.text != "{") {
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
						
						ASTnodes.push({
							kind: "matchCase",
							location: identifier.location,
							name: name,
							types: types,
							codeBlock: codeBlock,
						});
					} else {
						context.i--;
						ASTnodes.pop();
						ASTnodes.push(parseOperators(context, left, 0));
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
				
				ASTnodes.push({
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
			return ASTnodes;
		}
		
		if (context.tokens[context.i] && next(context).type == TokenKind.separator && next(context).text == "(") {
			continue;
		}
		
		if (mode == ParserMode.singleNoOperatorContinue) {
			earlyReturn();
			return ASTnodes;
		}
		
		if (context.tokens[context.i] && next(context).type == TokenKind.operator) {
			if (mode == ParserMode.singleNoEqualsOperatorContinue && next(context).text == "=") {
				earlyReturn();
				return ASTnodes;
			}
			continue;
		}
		
		if (mode == ParserMode.singleNoEqualsOperatorContinue) {
			earlyReturn();
			return ASTnodes;
		}
		
		if (mode == ParserMode.singleNoNewInstanceContinue) {
			earlyReturn();
			return ASTnodes;
		}
		
		if (context.tokens[context.i] && next(context).type == TokenKind.separator && next(context).text == "{") {
			const left = ASTnodes.pop();
			if (left) {
				forward(context);
				const codeBlock = parse(context, ParserMode.normal, "}");
				
				ASTnodes.push({
					kind: "instance",
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
			return ASTnodes;
		}
		
		if (endAt != null) {
			const nextToken = next(context);
			
			if (nextToken.type == TokenKind.separator) {
				if (nextToken.text == ")" || nextToken.text == "}" || nextToken.text == "]") {
					if (endAt == nextToken.text) {
						context.i++;
						return ASTnodes;
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
	
	return ASTnodes;
}
import Foundation

func parse(_ parserContext: inout ParserContext, _ tokens: [token], _ endAfter1Token: Bool = false) -> [any SyntaxProtocol] {
	var AST: [any SyntaxProtocol] = []
	
	var token = tokens[parserContext.index]
	
	while (true) {
		if (parserContext.index >= tokens.count) {
			break
		}
		
		token = tokens[parserContext.index]
		
		if (token.name == "word") {
			switch (token.value) {
			case "function": parse_function()
				break
			case "include": parse_include()
				break
			case "return": parse_return()
				break
			case "var": parse_variable_definition()
				break
			default:
				if (tokens[parserContext.index+1].name == "separator" && tokens[parserContext.index+1].value == "(") {
					// eat the word and separator
					parserContext.index += 2
					
					let arguments = parse(&parserContext, tokens)
					
					AST.append(SyntaxCall(lineNumber: token.lineNumber, start: token.start, end: token.end, name: token.value, arguments: arguments))
				} else if (tokens[parserContext.index+1].name == "operator" && tokens[parserContext.index+1].value == "=") {
					// eat the word and operator
					parserContext.index += 2
					
					let expression = parse(&parserContext, tokens, true)
					
					AST.append(SyntaxAssignment(lineNumber: token.lineNumber, start: token.start, end: token.end, name: token.value, expression: expression))
				} else {
					AST.append(SyntaxWord(lineNumber: token.lineNumber, start: token.start, end: token.end, value: token.value))
				}
			}
		}
		
		else if (token.name == "string") {
			AST.append(SyntaxString(lineNumber: token.lineNumber, start: token.start, end: token.end, value: token.value))
		}
		
		else if (token.name == "number") {
			AST.append(SyntaxNumber(lineNumber: token.lineNumber, start: token.start, end: token.end, value: token.value))
		}
		
		else if (token.name == "separator") {
			if (token.value == ")") {
				break
			}
			
			else if (token.value == "}") {
				break
			}
			
			else if (token.value == ":") {
				compileError("unexpected colon", token.lineNumber, token.start, token.end)
			}
			
			else {
				compileError("unexpected separator \(token)", token.lineNumber, token.start, token.end)
			}
		}
		
		else {
			compileError("unknown token name \(token)", token.lineNumber, token.start, token.end)
		}
		
		if (endAfter1Token) {
			break
		}
		
		parserContext.index += 1
	}
	
	return AST
	
	func parse_function() {
		let nameToken = getNextToken()
		if (nameToken.name != "word") {
			compileError("function definition expected a name, example: `function main(): Int {}`", token.lineNumber, token.start, token.end)
		}
		
		let openingParentheses = getNextToken()
		if (openingParentheses.name != "separator" || openingParentheses.value != "(") {
			compileError("function definition expected an openingParentheses, example: `function main(): Int {}`", token.lineNumber, token.start, token.end)
		}
		parserContext.index += 1
		let arguments = parse(&parserContext, tokens)
		
		let colonToken = getNextToken()
		if (colonToken.name != "separator" || colonToken.value != ":") {
			compileError("function definition expected a colon, example: `function main(): Int {}`", token.lineNumber, token.start, token.end)
		}
		
		let returnType = parse_type()
		
		let openingBracket = getNextToken()
		if (openingBracket.name != "separator" || openingBracket.value != "{") {
			compileError("function definition expected an openingBracket, example: `function main(): Int {}`", token.lineNumber, token.start, token.end)
		}
		parserContext.index += 1
		let codeBlock = parse(&parserContext, tokens)
		
		if (arguments.count != 0) {
			compileError("function arguments are not available right now", token.lineNumber, token.start, token.end)
		}
		
		AST.append(SyntaxFunction(lineNumber: token.lineNumber, start: token.start, end: token.end, name: nameToken.value, arguments: [], codeBlock: codeBlock, returnType: returnType))
	}
	
	func parse_include() {
		let openingBracket = getNextToken()
		if (openingBracket.name != "separator" || openingBracket.value != "{") {
			compileError("function definition expected an openingBracket, example: `function main(): Int {}`", token.lineNumber, token.start, token.end)
		}
		parserContext.index += 1
		let names = parse(&parserContext, tokens)
		
		AST.append(SyntaxInclude(lineNumber: token.lineNumber, start: token.start, end: token.end, names: names))
	}
	
	func parse_return() {
		parserContext.index += 1
		let value = parse(&parserContext, tokens, true)
		
		AST.append(SyntaxReturn(lineNumber: token.lineNumber, start: token.start, end: token.end, value: value[0]))
	}
	
	func parse_variable_definition() {
		let nameToken = getNextToken()
		if (nameToken.name != "word") {
			compileError("variable definition expected a name`", token.lineNumber, token.start, token.end)
		}
		
		let colonToken = getNextToken()
		if (colonToken.name != "separator" || colonToken.value != ":") {
			compileError("variable definition expected a colon", token.lineNumber, token.start, token.end)
		}
		
		let variableType = parse_type()
		
		AST.append(SyntaxDefinition(lineNumber: token.lineNumber, start: token.start, end: token.end, name: nameToken.value, type: variableType))
	}
	
	func parse_type() -> any buildType {
		let typeToken = getNextToken()
		
		if (typeToken.name != "word") {
			compileError("type expected a word but got a \(typeToken.name)`", typeToken.lineNumber, typeToken.start, typeToken.end)
		}
		
		let variable = buildTypeSimple(typeToken.value)
		
		return variable
	}
	
	func getNextToken() -> token {
		parserContext.index += 1
		return tokens[parserContext.index]
	}
}

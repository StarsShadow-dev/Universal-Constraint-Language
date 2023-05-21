import Foundation

func lex() -> [token] {
	var tokens: [token] = []
	
	var line = 0
	var column = 0
	
	var index = 0
	
	while (true) {
		if (sourceCode.count <= index) {
			break
		}
		let char = sourceCode[sourceCode.index(sourceCode.startIndex, offsetBy: index)]
		
		let startColumn = column
		
		if (char == "\n") {
			line += 1
			column = 0
			
			index += 1
			continue
		}
		
		// space or tab
		else if (char == " " || char == "\t") {
			// do nothing
		}
		
		// words must start with a letter or an underscore
		else if (char.isLetter || char == "_") {
			
			let value = readWhile({ char in
				// words can have numbers after the first character
				return char.isLetter || char == "_" || char.isNumber
			})
			
			tokens.append(token(
				name: "word",
				lineNumber: line,
				start: startColumn,
				end: column,
				value: value
			))
		}
		
		// strings
		else if (char == "\"") {
			// eat the opening "
			column += 1
			index += 1
			
			var value = ""
			
			var escaped = false
			var continueLoop = true
			
			while (continueLoop) {
				if (sourceCode.count <= index) {
					column -= 1
					index -= 1
					continueLoop = false
				} else {
					let char = sourceCode[sourceCode.index(sourceCode.startIndex, offsetBy: index)]
					
					if (escaped) {
						escaped = false
						
						column += 1
						index += 1
						
						if (char == "n") {
							value.append("\n")
							continue
						} else if (char == "\"") {
							continue
						}
					}
					
					if (char == "\\") {
						escaped = true
						column += 1
						index += 1
					} else if (char != "\"") {
						value.append(char)
						if (char == "\n") {
							line += 1
							column = 0
						} else {
							column += 1
							index += 1
						}
					} else {
						column -= 1
						index -= 1
						continueLoop = false
					}
				}
			}
			
			tokens.append(token(
				name: "string",
				lineNumber: line,
				start: startColumn,
				end: column,
				value: value
			))
			
			// eat the closing "
			column += 1
			index += 1
		}
		
		else if (char.isNumber) {
			tokens.append(token(
				name: "number",
				lineNumber: line,
				start: startColumn,
				end: column,
				value: readWhile({ char in
					return char.isNumber || char == "."
				})
			))
		}
		
		else if (char == "(" || char == ")" || char == "{" || char == "}" || char == ":") {
			tokens.append(token(
				name: "separator",
				lineNumber: line,
				start: startColumn,
				end: column,
				value: String(char)
			))
		}
		
		else if (char == "=") {
			tokens.append(token(
				name: "operator",
				lineNumber: line,
				start: startColumn,
				end: column,
				value: String(char)
			))
		}
		
		// for comments
		else if (char == "/" && sourceCode[sourceCode.index(sourceCode.startIndex, offsetBy: index+1)] == "/") {
			let _ = readWhile({ char in
				return char != "\n"
			})
		}
		
		/*
			for multi line comments
		*/
		else if (char == "/" && sourceCode[sourceCode.index(sourceCode.startIndex, offsetBy: index+1)] == "*") {
			let _ = readWhile({ char in
				if (sourceCode.count <= index) {
					compileError("multi line comment never ended")
				}
				let next = sourceCode[sourceCode.index(sourceCode.startIndex, offsetBy: index+1)]
				return char != "*" || next != "/"
			})
			
			// eat the / and *
			column += 2
			index += 2
		}
		
		else {
			compileError("unexpected character: \(char)", line, column, column)
		}
		
		column += 1
		index += 1
	}
	
	return tokens
	
	func readWhile(_ function: (_ char: Character) -> Bool) -> String {
		var past = ""
		while (true) {
			if (sourceCode.count <= index) {
				column -= 1
				index -= 1
				return past
			} else {
				let char = sourceCode[sourceCode.index(sourceCode.startIndex, offsetBy: index)]
				
				if (function(char)) {
					past.append(char)
					if (char == "\n") {
						line += 1
						column = 0
					} else {
						column += 1
					}
					index += 1
				} else {
					column -= 1
					index -= 1
					return past
				}
			}
		}
	}
}

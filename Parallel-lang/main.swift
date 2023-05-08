import Foundation

// mode can be:
// build - build as executable
// run - build and then run
var mode: String = "run"
var outputFileName: String = "test"
var logEverything: Bool = true
var printProgress: Bool = true

var sourceCode: String = """
function main(): Int {
	putchar(97):
	putchar(98)
	putchar(99)

	return 0
}
"""

//; ModuleID = 'test'
//
//declare i32 @putchar(i32 noundef) #1
//
//define i32 @main() {
//entry:
//	%0 = call i32 @putchar(i32 noundef 97)
//	%1 = call i32 @putchar(i32 noundef 98)
//	%2 = call i32 @putchar(i32 noundef 99)
//	ret i32 0
//}

// these functions assume that ~/.zshrc exists and that it defines: llc and clang
func findLLCPath() throws -> String {
	let output = try Shell.exec("/bin/sh", with: ["-l", "-c", "source ~/.zshrc && which llc"])
	return output.stdout.components(separatedBy: "\n").first!
}
func findclangPath() throws -> String {
	let output = try Shell.exec("/bin/sh", with: ["-l", "-c", "source ~/.zshrc && which clang"])
	return output.stdout.components(separatedBy: "\n").first!
}

func progressLog(_ string: String) {
	if (printProgress) {
		print("\(string)")
	}
}

func compileError(_ message: String, _ lineNumber:Int? = nil, _ columnStart:Int? = nil, _ columnEnd:Int? = nil) {
	print("Compile Error:")
	print("\(message)")
	
	let lines = sourceCode.components(separatedBy: "\n")
	
	if let lineNumber {
		if (lineNumber-2 > lines.count) {
			print("\(lineNumber-2) | \(lines[lineNumber-2])")
		}
		if (lineNumber-1 > lines.count) {
			print("\(lineNumber-1) | \(lines[lineNumber-1])")
		}
		
		print("\(lineNumber) | \(lines[lineNumber])")
		if let columnStart, let columnEnd {
			print("\(String(repeating: "-", count: "\(lineNumber)".count))---\(String(repeating: "-", count: columnStart))\(String(repeating: "^", count: columnEnd - columnStart + 1))")
		} else {
			print("\(String(repeating: "-", count: "\(lineNumber)".count))---\(String(repeating: "^", count: lines[lineNumber].count))")
		}
		
		if (lineNumber+1 < lines.count) {
			print("\(lineNumber+1) | \(lines[lineNumber+1])")
		}
		if (lineNumber+2 < lines.count) {
			print("\(lineNumber+2) | \(lines[lineNumber+2])")
		}
	}
	
	abort()
}

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
		
		else if (char.isLetter) {
			
			let value = read_while({ char in
				return char.isLetter
			})
			
			tokens.append(token(name: "word",
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
			
			let value = read_while({ char in
				return char != "\""
			})
			
			tokens.append(token(name: "string",
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
			tokens.append(token(name: "number",
				lineNumber: line,
				start: startColumn,
				end: column,
				value: read_while({ char in
					return char.isNumber || char == "."
				})
			))
		}
		
		else if (char == "(" || char == ")" || char == "{" || char == "}" || char == ":") {
			tokens.append(token(name: "separator",
				lineNumber: line,
				start: startColumn,
				end: column,
				value: String(char)
			))
		}
		
		else if (char == "/" && sourceCode[sourceCode.index(sourceCode.startIndex, offsetBy: index+1)] == "/") {
			let _ = read_while({ char in
				return char != "\n"
			})
		}
		
		else {
			compileError("unexpected character: \(char)", line, column, column)
		}
		
		column += 1
		index += 1
	}
	
	return tokens
	
	func read_while(_ function: (_ char: Character) -> Bool) -> String {
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
				} else {
					column -= 1
					index -= 1
					return past
				}
			}
			
			column += 1
			index += 1
		}
	}
}

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
			case "return": parse_return()
				break
			default:
				if (tokens[parserContext.index+1].name == "separator" && tokens[parserContext.index+1].value == "(") {
					// eat the word and separator
					parserContext.index += 2
					
					let arguments = parse(&parserContext, tokens)
					
					AST.append(SyntaxCall(name: token.value, arguments: arguments))
				} else {
					AST.append(SyntaxWord(value: token.value))
				}
			}
		}
		
		else if (token.name == "string") {
			AST.append(SyntaxString(value: token.value))
		}
		
		else if (token.name == "number") {
			AST.append(SyntaxNumber(value: token.value))
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
		
		AST.append(SyntaxFunction(name: nameToken.value, arguments: arguments, codeBlock: codeBlock, returnValue: returnType))
	}
	
	func parse_return() {
		parserContext.index += 1
		let value = parse(&parserContext, tokens, true)
		
		AST.append(SyntaxReturn(value: value[0]))
	}
	
	func parse_type() -> any buildVariable {
		let typeToken = getNextToken()
		
		if (typeToken.name != "word") {
			compileError("type expected a word but got a \(typeToken.name)`", typeToken.lineNumber, typeToken.start, typeToken.end)
		}
		
		let variable = variableData()
		
		variable.type = typeToken.value
		
		return variable
	}
	
	func getNextToken() -> token {
		parserContext.index += 1
		return tokens[parserContext.index]
	}
}

func buildLLVM(_ builderContext: BuilderContext, _ inside: (any buildVariable)?, _ AST: [any SyntaxProtocol], _ newLevel: Bool) -> String {
	var LLVMSource: String = ""
	
	var index = 0
	
	if (newLevel) {
		builderContext.level += 1
		builderContext.variables.append([:])
	}
	
	while (true) {
		if (index >= AST.count) {
			break
		}

		let node = AST[index]

		if let node = node as? SyntaxFunction {
			let data = functionData()
			
			data.arguments = node.arguments
			
			builderContext.variables[builderContext.level][node.name] = data
		}

		index += 1
	}
	
	index = 0
	
	while (true) {
		if (index >= AST.count) {
			break
		}
		
		let node = AST[index]
		
		if let node = node as? SyntaxFunction {
			if (node.name == "main") {
				let function = builderContext.variables[builderContext.level][node.name]
				let build = buildLLVM(builderContext, function, node.codeBlock, true)
				
				LLVMSource.append("define i32 @main() {\nentry:")
				
				LLVMSource.append(build)
				
				LLVMSource.append("\n}")
			} else {
				compileError("only the main function is usable right now")
			}
		}
		
		else if let node = node as? SyntaxReturn {
			LLVMSource.append("\n\tret \(buildLLVM(builderContext, inside, [node.value], false))")
		}
		
		else if let node = node as? SyntaxNumber {
			LLVMSource.append("i32 \(node.value)")
		}
		
		else if let node = node as? SyntaxCall {
			if let inside = inside as? functionData {
				let string: String = buildLLVM(builderContext, inside, node.arguments, false)
				
				LLVMSource.append("\n\t%\(inside.instructionCount) = call i32 @putchar(\(string))")
				inside.instructionCount += 1
			} else {
				compileError("call outside of a function")
			}
		}
		
		else {
			compileError("unexpected node type \(node)")
		}
		
		index += 1
	}
	
	if (newLevel) {
		builderContext.level -= 1
		let _ = builderContext.variables.popLast()
	}
	
	return LLVMSource
}

func compile() throws {
	print("Current directory: \(FileManager.default.currentDirectoryPath)")
	
	progressLog("building...")
	
	let tokens = lex()
	
	if (logEverything) {
		print("tokens:\n\(tokens)")
	}
	
	var parserContext = ParserContext(index: 0)
	
	let abstractSyntaxTree = parse(&parserContext, tokens)
	
	if (logEverything) {
		print("abstractSyntaxTree:\n\(abstractSyntaxTree)")
	}
	
	let builderContext = BuilderContext(level: -1, variables: [])
	let inside: (any buildVariable)? = nil
	
	let LLVMSource = "declare i32 @putchar(i32 noundef) #1\n\n" + buildLLVM(builderContext, inside, abstractSyntaxTree, true)
	
	if (logEverything) {
		print("LLVMSource:\n\(LLVMSource)")
	}
	
	progressLog("build complete!")
	
	progressLog("creating object file...")
	let LLVMOutput = try Shell.exec(findLLCPath(), with: [
		// set the output type to be an object file ready for linking.
		"-filetype=obj",
		// write the output to the given file.
		"-o", outputFileName + ".o"
	], input: LLVMSource)
	
	if !LLVMOutput.stderr.isEmpty {
		fatalError("LLVM Errors: \n\(LLVMOutput.stderr)")
	}
	if LLVMOutput.exitCode != 0 {
		fatalError("LLVM failed with exit code: \(LLVMOutput.exitCode)")
	}
	
	progressLog("object file created!")
	
	progressLog("linking...")
	let clangOutput = try Shell.exec(findclangPath(), with: [
		outputFileName + ".o", "-o",
		// write the output to the given file.
		outputFileName
	], input: nil)
	
	if !clangOutput.stderr.isEmpty {
		fatalError("clang Errors: \n\(clangOutput.stderr)")
	}
	if clangOutput.exitCode != 0 {
		fatalError("clang failed with exit code: \(clangOutput.exitCode)")
	}
	
	progressLog("linking complete!")
	
	if (mode == "build") {
		progressLog("executable at \(FileManager.default.currentDirectoryPath + "/" + outputFileName)")
	} else if (mode == "run") {
		let runPath = FileManager.default.currentDirectoryPath + "/" + outputFileName
		progressLog("running program at \(runPath)\n")
		
		let run = try Shell.exec(runPath, with: [], input: nil)
		
		if !run.stderr.isEmpty {
			print("run stderr: \(run.stderr)")
		}
		print("run stdout: \(run.stdout)")
		print("run exitCode: \(run.exitCode)")
	}
}

try compile()

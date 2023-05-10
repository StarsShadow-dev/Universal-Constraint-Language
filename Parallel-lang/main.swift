import Foundation

// mode can be:
// build - build as executable
// run - build and then run
var mode: String = "run"
var outputFileName: String = "test"
var logEverything: Bool = true
var printProgress: Bool = true
var optimize: Bool = false

var toplevelLLVM = ""

var externalFunctions: [String:ExternalFunction] = [
	"putchar":ExternalFunction("declare i32 @putchar(i32 noundef) #1")
]

var sourceCode: String = """
//include { putchar }

function main(): Int32 {
	// return with exit code 0
	return 0
}
"""

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
		
		print("\(lineNumber) | \(lines[lineNumber].replacingOccurrences(of: "\t", with: " "))")
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

/// this function takes a message and something that conforms to hasLocation
func compileErrorWithHasLocation(_ message: String, _ location: any hasLocation) {
	compileError(message, location.lineNumber, location.start, location.end)
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
		
		// words must start with a letter or an underscore
		else if (char.isLetter || char == "_") {
			
			let value = readWhile({ char in
				// words can have numbers after the first character
				return char.isLetter || char == "_" || char.isNumber
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
			
			let value = readWhile({ char in
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
				value: readWhile({ char in
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
						index += 1
					}
				} else {
					column -= 1
					index -= 1
					return past
				}
			}
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
			case "include": parse_include()
				break
			case "return": parse_return()
				break
			default:
				if (tokens[parserContext.index+1].name == "separator" && tokens[parserContext.index+1].value == "(") {
					// eat the word and separator
					parserContext.index += 2
					
					let arguments = parse(&parserContext, tokens)
					
					AST.append(SyntaxCall(lineNumber: token.lineNumber, start: token.start, end: token.end, name: token.value, arguments: arguments))
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
		
		AST.append(SyntaxFunction(lineNumber: token.lineNumber, start: token.start, end: token.end, name: nameToken.value, arguments: arguments, codeBlock: codeBlock, returnType: returnType))
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

func buildLLVM(_ builderContext: BuilderContext, _ inside: (any buildVariable)?, _ AST: [any SyntaxProtocol], _ typeList: [any buildType]?, _ newLevel: Bool) -> String {
	
	if let typeList {
		if (AST.count > typeList.count) {
			compileError("not enough arguments")
		}
	}
	
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
			let data = functionData(node.arguments, node.returnType)
			
			builderContext.variables[builderContext.level][node.name] = data
		}
		
		else if let node = node as? SyntaxInclude {
			for name in node.names {
				if let name = name as? SyntaxWord {
					if let externalFunction = externalFunctions[name.value] {
						if (externalFunction.hasBeenDefined) {
							compileErrorWithHasLocation("external function \(name.value) has already been defined", name)
						}
						toplevelLLVM += externalFunction.LLVM + "\n"
						externalFunction.hasBeenDefined = true
					} else {
						compileErrorWithHasLocation("external function \(name.value) does not exist", name)
					}
				} else {
					compileErrorWithHasLocation("not a word in an include statement", name)
				}
			}
		}
		
		else {
			if (builderContext.level == 0) {
				compileErrorWithHasLocation("only functions and include are allowed at top level", node)
			}
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
			let function = builderContext.variables[builderContext.level][node.name]
			let build = buildLLVM(builderContext, function, node.codeBlock, nil, true)
			
			LLVMSource.append("\ndefine i32 @\(node.name)() {\nentry:")
			
			LLVMSource.append(build)
			
			LLVMSource.append("\n}")
		}
		
		else if let _ = node as? SyntaxInclude {
			
		}
		
		else if let node = node as? SyntaxReturn {
			if let inside = inside as? functionData {
				LLVMSource.append("\n\tret \(buildLLVM(builderContext, inside, [node.value], [inside.returnType], false))")
			} else {
				compileErrorWithHasLocation("return outside of a function", node)
			}
		}
		
		else if let node = node as? SyntaxCall {
			if let inside = inside as? functionData {
				let string: String = buildLLVM(builderContext, inside, node.arguments, [], false)
				
				LLVMSource.append("\n\t%\(inside.instructionCount) = call i32 @\(node.name)(\(string))")
				inside.instructionCount += 1
			} else {
				compileErrorWithHasLocation("call outside of a function", node)
			}
		}
		
		else if let node = node as? SyntaxNumber {
			if let type = typeList![index] as? buildTypeSimple {
				if (type.name == "Int32") {
					LLVMSource.append("i32 \(node.value)")
				} else {
					compileErrorWithHasLocation("expected type Int32 but got type \(type.name)", node)
				}
			} else {
				compileErrorWithHasLocation("not buildTypeSimple?", node)
			}
		}
		
		else {
			compileErrorWithHasLocation("unexpected node type \(node)", node)
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
	
	print("topLevelLLVM", toplevelLLVM)
	var LLVMSource = buildLLVM(builderContext, inside, abstractSyntaxTree, nil, true)
	
	LLVMSource = toplevelLLVM + "\n" + LLVMSource
	
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

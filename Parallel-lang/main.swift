import Foundation

struct token {
	var name: String
	var value: String
}

// mode can be:
// build - build as executable
// run - build and then run
var mode: String = "run"
var outputFileName: String = "test"
var logEverything: Bool = true
var printProgress: Bool = true
var compileFailed: Bool = false

var sourceCode: String = """
function() {
	"testing"
}
"""

// LLVMSource puts "abc" to stdout
var LLVMSource: String = """
; ModuleID = 'test'

define i32 @main() {
entry:
	%0 = call i32 @putchar(i32 noundef 97)
	%1 = call i32 @putchar(i32 noundef 98)
	%2 = call i32 @putchar(i32 noundef 99)
	ret i32 0
}

declare i32 @putchar(i32 noundef) #1
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
		
		print("index: \(index) char: \(char)")
		
		if (char == "\n") {
			line += 1
			column = 0
		}
		
		// space or tab
		else if (char == " " || char == "\t") {
			// do nothing
		}
		
		// strings
		else if (char == "\"") {
			// eat the opening "
			column += 1
			index += 1
			
			tokens.append(token(name: "string", value: read_while({ char in
				return char != "\""
			})))
		}
		
		else if (char.isLetter) {
			tokens.append(token(name: "word", value: read_while({ char in
				return char.isLetter
			})))
		}
		
		else if (char.isNumber) {
			tokens.append(token(name: "number", value: read_while({ char in
				return char.isNumber || char == "."
			})))
		}
		
		else if (char == "(" || char == ")" || char == "{" || char == "}" || char == ":") {
			tokens.append(token(name: "separator", value: String(char)))
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

func parse(_ tokens: [token]) -> [String:Any] {
	var AST: [String:Any] = [:]
	
	return AST
}

func buildLLVM(_ AST: [String:Any]) {
	
}

func compile() throws {
	print("Current directory: \(FileManager.default.currentDirectoryPath)")
	
	let tokens = lex()
	
	if (logEverything) {
		print("tokens:\n\(tokens)")
	}
	
	let abstractSyntaxTree = parse(tokens)
	
	if (logEverything) {
		print("abstractSyntaxTree:\n\(abstractSyntaxTree)")
	}
	
	buildLLVM(abstractSyntaxTree)
	
	if (logEverything) {
		print("LLVMSource:\n\(LLVMSource)")
	}
	
	return;
	
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
	let clang = try Shell.exec(findclangPath(), with: [
		outputFileName + ".o", "-o",
		// write the output to the given file.
		outputFileName
	], input: nil)
	
	if !clang.stderr.isEmpty {
		fatalError("clang Errors: \n\(clang.stderr)")
	}
	if clang.exitCode != 0 {
		fatalError("clang failed with exit code: \(clang.exitCode)")
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

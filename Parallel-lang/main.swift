import Foundation

// mode can be:
// build - build as executable
// run - build and then run
var mode: String = "run"
var outputFileName: String = "test"
var logEverything: Bool = true
var printProgress: Bool = true
var compileFailed: Bool = false

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

func compileError(_ message: String) {
	print("Compile Error:")
	print("\(message)")
	
	abort()
}

func lex(_ string: String) -> [String] {
	var tokens: [String] = []
	
	var past = ""
	
	for (_, char) in string.enumerated() {
		print("char: \(char)")
		if (char.isLetter) {
			past.append(char)
		}
	}
	
//	if (!past.isEmpty) {
//		compileError("past")
//	}
	
	return tokens
}

func parse(_ tokens: [String]) -> [String:Any] {
	var AST: [String:Any] = [:]
	
	return AST
}

func buildLLVM(_ AST: [String:Any]) {
	
}

func compile() throws {
	print("Current directory: \(FileManager.default.currentDirectoryPath)")
	
	let tokens = lex("function ")
	
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

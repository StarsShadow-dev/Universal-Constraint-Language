#if !os(macOS)
	#error("the compiler can only be built on macOS (currently)")
#endif

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
	"putchar":ExternalFunction("declare i32 @putchar(i32 noundef) #1", functionData("putchar", [buildTypeSimple("Int32")], buildTypeSimple("Void")))
]

var sourceCode: String = """
include { putchar }

function main(): Int32 {
	putchar(97)
	putchar(98)
	putchar(99)
	
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

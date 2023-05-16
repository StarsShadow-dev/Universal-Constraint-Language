#if !os(macOS)
	#error("the compiler can only be built on macOS (currently)")
#endif

import Foundation

var globalConfig: GlobalConfig = GlobalConfig()
var config: Config = Config()

// mode can be:
// build - build as executable
// run - build and then run
let mode: String = "run"
let logEverything: Bool = true
let printProgress: Bool = true
let optimize: Bool = false

var toplevelLLVM = ""

var LLVMTypeMap: [String:String] = [
	"Void":"void",
	"Int32":"i32"
]

var constants: [String] = []

var externalFunctions: [String:ExternalFunction] = [
	"putchar":ExternalFunction("declare i32 @putchar(i32 noundef) #1", functionData("putchar", "putchar", [buildTypeSimple("Int32")], buildTypeSimple("Int32"))),
	"printf":ExternalFunction("declare i32 @printf(ptr noundef, ...) #1", functionData("printf", "printf", [buildTypeSimple("String")], buildTypeSimple("Int32"))),
]

var sourceCode: String = ""

func progressLog(_ string: String) {
	if (printProgress) {
		print("\(string)")
	}
}

func compileError(_ message: String, _ lineNumber:Int? = nil, _ columnStart:Int? = nil, _ columnEnd:Int? = nil) -> Never {
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
func compileErrorWithHasLocation(_ message: String, _ location: any hasLocation) -> Never {
	compileError(message, location.lineNumber, location.start, location.end)
}

func setUpFolder() throws {
	let currentDirectoryURL = URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
	let rolderURL = URL.homeDirectory.appending(path: ".Parallel_Lang")

	if !FileManager.default.fileExists(atPath: rolderURL.path()) {
		if (logEverything) {
			print("no folder at \(rolderURL.path()), creating folder...")
		}
		do {
			try FileManager.default.createDirectory(atPath: rolderURL.path(), withIntermediateDirectories: false, attributes: nil)
		} catch {
			print(error.localizedDescription)
			abort()
		}
		if (logEverything) {
			print("folder created!")
		}
	} else {
		if (logEverything) {
			print("folder at \(rolderURL.path()) already exists")
		}
	}
	
	var globalConfigJSON_data = Data()
	
	if !FileManager.default.fileExists(atPath: rolderURL.appending(path: "config.json").path()) {
		globalConfigJSON_data = """
{
	"LLC_path": "",
	"clang_path": ""
}
""".data(using: .utf8)!
		FileManager.default.createFile(atPath: rolderURL.appending(path: "config.json").path(), contents: globalConfigJSON_data)
	} else {
		globalConfigJSON_data = try Data(contentsOf: rolderURL.appending(path: "config.json"))
	}
	
	if (logEverything) {
		print("globalConfig: \(String(decoding: globalConfigJSON_data, as: UTF8.self))")
	}
	
	globalConfig = try JSONDecoder().decode(GlobalConfig.self, from: globalConfigJSON_data)
	
	if (globalConfig.LLC_path.count == 0) {
		print("globalConfig LLVM_path is empty")
		exit(1)
	}
	
	if (globalConfig.clang_path.count == 0) {
		print("globalConfig clang_path is empty")
		exit(1)
	}
	
	var configJSON_data: Data
	
	do {
		configJSON_data = try Data(contentsOf: currentDirectoryURL.appending(path: "config.json"))
	} catch {
		print("\(error.localizedDescription)")
		exit(1)
	}
	
	if (logEverything) {
		print("config: \(String(decoding: configJSON_data, as: UTF8.self))")
	}
	
	do {
		config = try JSONDecoder().decode(Config.self, from: configJSON_data)
	} catch {
		print("\(error.localizedDescription)")
		exit(1)
	}
	
	if (config.name.count == 0) {
		print("config name is empty")
		exit(1)
	}
	
	if (config.entry_path.count == 0) {
		print("config entry_path is empty")
		exit(1)
	}
	
	if (config.build_directory.count == 0) {
		print("config build_directory is empty")
		exit(1)
	}
	
	do {
		sourceCode = try String(contentsOf: currentDirectoryURL.appending(path: config.entry_path))
	} catch {
		print("\(error.localizedDescription)")
		exit(1)
	}
}

func compile() throws {
	if (logEverything) {
		print("Current directory: \(FileManager.default.currentDirectoryPath)")
	}
	
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
	let inside: functionData? = nil
	
	var LLVMSource = buildLLVM(builderContext, inside, nil, abstractSyntaxTree, nil, true)
	
	var constantsString = ""
	
	for (index, constant) in zip(constants.indices, constants) {
		// constant.count + 1 because we are adding a null bite at the end of the constant
		constantsString += "@.const.\(index) = private unnamed_addr constant [\(constant.count + 1) x i8] c\"\(constant)\\00\", align 1\n"
	}
	
	LLVMSource = constantsString + toplevelLLVM + "\n" + LLVMSource
	
	if (logEverything) {
		print("LLVMSource:\n\(LLVMSource)")
	}
	
	progressLog("build complete!")
	
	progressLog("creating object file...")
	let LLVMOutput = try Shell.exec(globalConfig.LLC_path, with: [
		// set the output type to be an object file ready for linking.
		"-filetype=obj",
		// write the output to the given file.
		"-o", config.build_directory + "/" + "objectFile.o"
	], input: LLVMSource)
	
	if !LLVMOutput.stderr.isEmpty {
		fatalError("LLVM Errors: \n\(LLVMOutput.stderr)")
	}
	if LLVMOutput.exitCode != 0 {
		fatalError("LLVM failed with exit code: \(LLVMOutput.exitCode)")
	}
	
	progressLog("object file created!")
	
	progressLog("linking...")
	let clangOutput = try Shell.exec(globalConfig.clang_path, with: [
		config.build_directory + "/" + "objectFile.o", "-o",
		// write the output to the given file.
		config.build_directory + "/" + config.name
	], input: nil)
	
	if !clangOutput.stderr.isEmpty {
		fatalError("clang Errors: \n\(clangOutput.stderr)")
	}
	if clangOutput.exitCode != 0 {
		fatalError("clang failed with exit code: \(clangOutput.exitCode)")
	}
	
	progressLog("linking complete!")
	
	let runPath = FileManager.default.currentDirectoryPath + "/" + config.build_directory + "/" + config.name
	
	if (mode == "build") {
		progressLog("executable at \(runPath)")
	} else if (mode == "run") {
		progressLog("running program at \(runPath)\n")
		
		let run = try Shell.exec(runPath, with: [], input: nil)
		
		if !run.stderr.isEmpty {
			print("run stderr: \(run.stderr)")
		}
		print("run stdout: \(run.stdout)")
		print("run exitCode: \(run.exitCode)")
	}
}

//print(CommandLine.arguments)

try setUpFolder()

try compile()

import Foundation

public enum Shell {
	public struct Output: Hashable {
		public var stdout: String
		public var stderr: String
		public var exitCode: Int32
	}
	
	public static func exec(
		_ command: String,
		with arguments: [String] = [],
		input: String? = nil
	) throws -> Output {
		let process = Process()
		process.launchPath = command
		process.arguments = arguments
		let stdoutPipe = Pipe()
		let stderrPipe = Pipe()
		let stdinPipe: Pipe?
		if input != nil {
			stdinPipe = Pipe()
			process.standardInput = stdinPipe
		}
		else {
			stdinPipe = nil
		}
		process.standardOutput = stdoutPipe
		process.standardError = stderrPipe
		process.launch()
		if let input, let stdinPipe {
			try stdinPipe.fileHandleForWriting.write(contentsOf: Data(input.utf8))
			try stdinPipe.fileHandleForWriting.close()
		}
		process.waitUntilExit()
		let stdoutData = stdoutPipe.fileHandleForReading.readDataToEndOfFile()
		let stderrData = stderrPipe.fileHandleForReading.readDataToEndOfFile()
		return Output(
			// If either of these decode steps fail at `.utf8` decoding,
			// just fall back to `.ascii`.
			stdout: String(data: stdoutData, encoding: .utf8) ?? String(data: stdoutData, encoding: .ascii)!,
			stderr: String(data: stderrData, encoding: .utf8) ?? String(data: stderrData, encoding: .ascii)!,
			exitCode: process.terminationStatus
		)
	}
}

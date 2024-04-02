// A very weird way to end compilation.
// I throw "__do nothing__". (the exception is caught in main.ts, and nothing is done with it)
// I would use exit, but apparently some standard out functions can get broken if you use exit.
// Instead, I use process.exitCode which should not interrupt any standard out functions.

import { stderr } from "process";

import { SourceLocation } from "./types";
import utilities from "./utilities";

const lineNumberPadding = 4;

export type Indicator = {
	location: SourceLocation,
	msg: string,
}

// TODO: This can read the same file twice, if there are two indicators in a file.
export function displayIndicator(indicator: Indicator) {
	if (indicator.location != "builtin") {
		const text = utilities.readFile(indicator.location.path);
		// console.log("compileError location:", location);
		
		stderr.write(`at ${indicator.location.path}:${indicator.location.line}\n\n`);
		
		let i = 0;
		let line = 1;
		
		function writeLine() {
			if (indicator.location != "builtin") {
				stderr.write(line.toString().padStart(lineNumberPadding, "0"));
				stderr.write(" |");
				for (; i < text.length; i++) {
					if (text[i] == "\n") line++;
					
					if (line != indicator.location.line) continue;
					
					stderr.write(text[i]);
				}
				stderr.write("\n");
			} else {
				utilities.unreachable();
			}
		}
		
		for (; i < text.length; i++) {
			if (text[i] == "\n") line++;
			
			if (line == indicator.location.line) {
				if (text[i] == "\n") i++;
				writeLine();
				stderr.write(`${indicator.msg}\n`);
			}
		}
		
		stderr.write(`\n`);	
	} else {
		stderr.write(`at builtin\n\n`);
	}
}

export class CompileError {
	msg: string
	private indicators: Indicator[]
	
	constructor(msg: string) {
		this.msg = msg;
		this.indicators = []
	}
	
	public indicator(location: SourceLocation, msg: string): CompileError {
		this.indicators.push({
			location: location,
			msg: msg,
		});
		return this;
	}
	
	public fatal(): never {
		stderr.write(`error: ${this.msg}\n`);
		for (const indicator of this.indicators) {
			displayIndicator(indicator);
		}
		process.exitCode = 1;
		throw "__do nothing__";
	}
}
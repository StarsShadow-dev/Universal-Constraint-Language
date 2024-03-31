// A very weird way to end compilation.
// I throw "__do nothing__". (the exception is caught in main.ts, and nothing is done with it)
// I would use exit, but apparently some standard out functions can get broken if you use exit.
// Instead, I use process.exitCode which should not interrupt any standard out functions.

import { exit, stderr } from 'process';

import { SourceLocation } from './types';
import utilities from './utilities';

const lineNumberPadding = 4;

// TODO: This can read the same file twice, if there are two indicators in a file.
function displayIndicator(location: SourceLocation, msg: string) {
	const text = utilities.readFile(location.path);
	// console.log("compileError location:", location);
	
	stderr.write(`at ${location.path}:${location.line}\n\n`);
	
	let i = 0;
	let line = 1;
	
	function writeLine() {
		stderr.write(line.toString().padStart(lineNumberPadding, "0"));
		stderr.write(" |");
		for (; i < text.length; i++) {
			if (text[i] == "\n") line++;
			
			if (line != location.line) continue;
			
			stderr.write(text[i]);
		}
		stderr.write("\n");
	}
	
	for (; i < text.length; i++) {
		if (text[i] == "\n") line++;
		
		if (line != location.line) continue;
		
		i++;
		writeLine();
		stderr.write(`${msg}\n`);
	}
	
	stderr.write(`\n`);
}

export class CompileError {
	private msg: string
	private indicators: {
		location: SourceLocation,
		msg: string,
	}[]
	
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
			displayIndicator(indicator.location, indicator.msg);
		}
		process.exitCode = 1;
		throw "__do nothing__";
	}
}
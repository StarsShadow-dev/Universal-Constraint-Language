// A very weird way to end compilation.
// I need to wait for fs.readFile, but waiting would cause the compiler to continue.
// So I throw "__do nothing__". (the exception is caught in main.ts, and nothing is done with it)

// I would use exit, but apparently some standard out functions can get broken if you use exit.
// Instead, I use process.exitCode which should not interrupt any standard out functions.

import * as fs from 'fs/promises';
import { exit, stdout } from 'process';

import { SourceLocation } from './types';

async function _compileError(location: SourceLocation, msg: string, indicator: string) {
	const text = await fs.readFile(location.path, { encoding: 'utf8' });
	console.log("compileError location:", location);
	console.log("msg:", msg);
	
	let i = 0;
	let line = 1;
	
	function writeLine() {
		stdout.write("    ");
		for (; i < text.length; i++) {
			if (text[i] == "\n") line++;
			
			if (line != location.line) continue;
			
			stdout.write(text[i]);
		}
		stdout.write("\n");
	}
	
	for (; i < text.length; i++) {
		if (text[i] == "\n") line++;
		
		if (line != location.line) continue;
		
		i++;
		writeLine();
	}
	
	process.exitCode = 1;
}

export function compileError(location: SourceLocation, msg: string, indicator: string): never {
	_compileError(location, msg, indicator);
	throw "__do nothing__";
}
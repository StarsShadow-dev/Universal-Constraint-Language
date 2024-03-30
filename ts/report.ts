// A very weird way to end compilation.
// I need to wait for fs.readFile, but waiting would cause the compiler to continue.
// So I throw "__do nothing__". (the exception is caught in main.ts, and nothing is done with it)

import * as fs from 'fs/promises';
import { exit, stdout } from 'process';

import { SourceLocation } from './types';

async function _compileError(location: SourceLocation, msg: string, indicator: string) {
	const text = await fs.readFile(location.path, { encoding: 'utf8' });
	console.log("compileError location:", location);
	console.log("msg:", msg);
	for (let i = 0; i < text.length; i++) {
		stdout.write(text[i]);
	}
	exit(1);
}

export function compileError(location: SourceLocation, msg: string, indicator: string): never {
	_compileError(location, msg, indicator);
	throw "__do nothing__";
}
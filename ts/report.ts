import { exit } from 'process';
import { SourceLocation } from './types';

export function compileError(location: SourceLocation, msg: string) {
	console.log("compileError location", location);
	console.log("msg", msg);
	exit(1);
}
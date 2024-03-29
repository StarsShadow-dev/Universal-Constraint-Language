import * as fs from 'fs/promises';

export async function compileFile(filePath: string) {
	const text = await fs.readFile(filePath, { encoding: 'utf8' });
	
	console.log("text:", text);
}

export function test() {
	return "from test";
}
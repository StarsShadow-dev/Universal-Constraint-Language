import { compileFile } from "./compiler";

(async () => {
	await compileFile("test/main.llcl");
})();
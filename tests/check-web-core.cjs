const fs=require('fs'),vm=require('vm'),path=require('path');
const root=path.resolve(__dirname,'..');
const source=fs.readFileSync(path.join(root,'web/data/compression/extract7z.js'),'utf8');
(async()=>{
 for(const variant of ['snes9x-wasm','snes9x-legacy-wasm']){
 const files={};const ctx={Module:{ENVIRONMENT:'SHELL'},console:{log(){},warn(){},error:console.error},print(){},printErr:console.error,postMessage:m=>{if(m.t===2)files[m.file]=Buffer.from(m.data);},close(){},setTimeout,clearTimeout};
 vm.createContext(ctx);vm.runInContext(source,ctx);ctx.onmessage({data:new Uint8Array(fs.readFileSync(path.join(root,'web/data/cores',variant+'.data')))});
 console.log(variant,Object.keys(files));
 if(!Object.keys(files).some(f=>f.endsWith('.wasm')))throw Error('No WASM extracted');
 for(const [name,data]of Object.entries(files)){
  if(name.endsWith('.wasm')){await WebAssembly.compile(data);console.log('Valid WebAssembly:',data.length,'bytes');}
  if(name==='license.txt')fs.writeFileSync(path.join(root,'web/licenses/'+variant+'-bundled-license.txt'),data);
  if(name.endsWith('.js'))new vm.Script(data.toString());
  if(name==='core.json'||name==='build.json')console.log(name,JSON.parse(data));
 }
 }
 const rom=fs.readFileSync(path.join(root,'bot-manager.sfc')),web=fs.readFileSync(path.join(root,'web/bot-manager.sfc'));if(!rom.equals(web))throw Error('Web ROM differs');
 console.log('Both bundled SNES cores decompress and validate; browser ROM is byte-identical.');
})().catch(e=>{console.error(e);process.exitCode=1});

const {chromium}=require('../toolchain/playwright/package');
const fs=require('fs'),path=require('path');
(async()=>{
 const browser=await chromium.launch({executablePath:process.env.CHROMIUM_PATH,headless:true,env:{...process.env,LD_LIBRARY_PATH:path.resolve(__dirname,'../toolchain/browser-libs/usr/lib/x86_64-linux-gnu')},args:['--no-sandbox','--disable-dev-shm-usage']});
 const page=await browser.newPage({viewport:{width:1024,height:1000}});const errors=[],requests=[];
 page.on('pageerror',e=>errors.push(e.message));page.on('console',m=>{if(m.type()==='error')console.log('console:',m.text())});
 const root=path.resolve(__dirname,'../web');
 await page.route('**/*',async route=>{const u=new URL(route.request().url());requests.push(u.href);if(u.hostname!=='game.test'){console.log('EXTERNAL BLOCKED',u.href);return route.abort();}let p=path.join(root,decodeURIComponent(u.pathname==='/'?'/index.html':u.pathname));if(!p.startsWith(root)||!fs.existsSync(p)){console.log('MISSING',p);return route.fulfill({status:404,body:'not found'});}let ext=path.extname(p),type={'.js':'application/javascript','.html':'text/html','.css':'text/css','.json':'application/json','.wasm':'application/wasm','.png':'image/png','.svg':'image/svg+xml'}[ext]||'application/octet-stream';await route.fulfill({body:fs.readFileSync(p),contentType:type});});
 await page.goto('http://game.test/');await page.waitForTimeout(2000);console.log('buttons',await page.locator('button').allTextContents());
 await page.getByText('Start Game',{exact:false}).first().click();await page.waitForTimeout(8000);await page.screenshot({path:path.resolve(__dirname,'../shots/web-desktop.png')});
 console.log('errors',errors,'requests',requests.length,'canvas',await page.locator('canvas').count());console.log('body', (await page.locator('body').innerText()).slice(-1600));
 await page.setViewportSize({width:390,height:844});await page.screenshot({path:path.resolve(__dirname,'../shots/web-phone.png')});console.log('overflow',await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth));
 await browser.close();if(errors.length)process.exitCode=1;
})().catch(e=>{console.error(e);process.exitCode=1});

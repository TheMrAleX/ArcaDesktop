.pragma library
function fold(text) {
    let result=""
    for(let c of String(text||"")){let base=c.normalize("NFD").charAt(0).toLowerCase();result+=base==='.'||base==='_'?' ':base}
    return result
}
function containsWords(text,query){let words=fold(query).split(' ').filter(w=>w.trim().length);let value=fold(text);return words.length>0&&words.every(w=>value.indexOf(w)>=0)}
function escape(text){return String(text||"").replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;")}
function highlight(text,query){text=String(text||"");let value=fold(text),marks=new Array(text.length).fill(false);for(let word of fold(query).split(' ').filter(w=>w.trim().length)){let at=value.indexOf(word);while(at>=0){for(let n=at;n<Math.min(text.length,at+word.length);++n)marks[n]=true;at=value.indexOf(word,at+word.length)}}let result="",start=0;while(start<text.length){let end=start+1;while(end<text.length&&marks[end]===marks[start])++end;let part=escape(text.slice(start,end));result+=marks[start]?'<font color="#8B9CFF">'+part+'</font>':part;start=end}return result}
function distance(a,b){a=fold(a).trim();b=fold(b).trim();if(!a||!b)return 1;let prev=Array.from({length:b.length+1},(_,n)=>n);for(let i=1;i<=a.length;++i){let current=[i];for(let j=1;j<=b.length;++j)current[j]=Math.min(current[j-1]+1,prev[j]+1,prev[j-1]+(a[i-1]===b[j-1]?0:1));prev=current}return prev[b.length]/Math.max(a.length,b.length)}
function closest(query,items){if(!query.trim())return {};let best={},score=0.42;for(let item of items){let title=item.title||"",candidate=Math.min(distance(query,title),distance(query,title.slice(0,query.length+2))+0.05);if(candidate<score){score=candidate;best=item}}return best}
function results(query,scope,movies,shows,files){
    query=query.trim();if(!query)return []
    let result=[]
    function append(label,items,display){for(let item of items)result.push(Object.assign({},item,{_group:label+" · "+items.length,_display:display(item)}))}
    if(scope==='Todo'||scope==='Películas')append('Películas',movies.filter(f=>containsWords(f.title,query)).sort((a,b)=>a.title.length-b.title.length),f=>f.title)
    if(scope==='Todo'||scope==='Series'){
        append('Series',shows.filter(f=>containsWords(f.title,query)),f=>f.title)
        if(query.length>=2)append('Episodios',files.filter(f=>f.kind==='EPISODE'&&!f.hidden&&!f.duplicate&&!f.spoilerHidden&&f.episodeTitle&&containsWords(f.episodeTitle,query)).slice(0,30),f=>f.episodeTitle)
    }
    if(query.length>=2&&(scope==='Todo'||scope==='Archivos'))append('Archivos',files.filter(f=>f.kind!=='IGNORED'&&f.kind!=='SUBTITLE'&&containsWords(f.name,query)).sort((a,b)=>(a.confidence>=80)-(b.confidence>=80)).slice(0,30),f=>f.name)
    return result
}

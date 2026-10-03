local M = require('ffi/mupdf')
local fixture = dofile('spec/base/unit/mupdf_relationship_fixture.lua')
describe('PDF annotation relationship preservation', function()
    local function bytes(path) local f=assert(io.open(path,'rb'));local s=f:read('*a');f:close();return s end
    local function objects(path)
        local t={};for n,b in bytes(path):gmatch('(%d+) 0 obj(.-)endobj') do t[tonumber(n)]=b end;return t
    end
    local function ref(obj,key) return tonumber(obj:match('/'..key..'%s+(%d+) 0 R')) end
    local function integer(obj,key) return tonumber(obj:match('/'..key..'%s+([%-0-9]+)')) end
    local function has(obj,text) assert(obj:find(text,1,true),text..' missing from '..obj) end
    for _,kind in ipairs({'popup','typeless_reply','chain','reply','group','parent','structure','nested','combined','tagged_popup','invalid_popup','invalid_reply','invalid_structure','cyclic'}) do
        it('preserves relationship graph '..kind, function()
        local dir=os.tmpname();os.remove(dir);assert(require('lfs').mkdir(dir))
        local file=dir..'/'..kind..'.pdf';local input=fixture(file,kind)
        local doc=M.openDocument(file);local p1=doc:openPage(1);local p2=doc:openPage(2)
        local base_count=#p1:getEmbeddedAnnotations()
        local entry=p1:getEmbeddedAnnotations()[1]
        local part={page=1,num=entry.source.num,gen=entry.source.gen,raw=entry.raw}
        local raw={};for k,v in pairs(entry.raw) do raw[k]=v end
        raw.quads={{40,40,140,40,40,60,140,60}}
        local ok,added=pcall(p2.createEmbeddedAnnotation,p2,raw,part)
        if kind:find('invalid',1,true) or kind=='cyclic' then
            assert(not ok,'invalid graph accepted');assert(p2:getEmbeddedAnnotations()==nil,'invalid graph left added part')
            assert(tostring(added):find('could not extend source annotation',1,true))
        else
            assert(ok,added);assert(added.page==2)
            local out=dir..'/'..kind..'-extended.pdf';doc:writeDocument(out)
            local o=objects(out);local dst=o[added.num];has(dst,'keep-source');assert(ref(dst,'P')==4)
            if kind=='popup' or kind=='combined' or kind=='tagged_popup' then
                local popup=assert(ref(dst,'Popup'));assert(popup~=8)
                assert(ref(o[popup],'Parent')==added.num);assert(ref(o[popup],'P')==4)
                has(o[popup],'keep-popup');has(o[popup],'/Open true')
                assert(ref(o[8],'Parent')==6);assert(ref(o[8],'P')==3)
                if kind=='tagged_popup' then
                    assert(integer(o[popup],'StructParent')==12)
                    has(o[10],'/Obj '..popup..' 0 R');has(o[10],'/Pg 4 0 R')
                end
            end
            if kind=='typeless_reply' or kind=='chain' or kind=='reply' or kind=='group' or kind=='combined' then
                assert(ref(dst,'IRT')==7);has(dst,kind=='group' and '/RT/Group' or '/RT/R')
            end
            if kind=='parent' or kind=='combined' then assert(ref(dst,'Parent')==7) end
            if kind=='structure' or kind=='nested' or kind=='combined' then
                assert(integer(o[6],'StructParent')==0);assert(integer(dst,'StructParent')==12)
                has(o[10],'/Obj '..added.num..' 0 R');has(o[10],'/Pg 4 0 R')
                has(o[kind=='nested' and 13 or 11],'12 10 0 R');has(o[11],'keep-tree')
                assert(integer(o[9],'ParentTreeNextKey')==13)
            end
            p2:close();p1:close();doc:close()
            doc=M.openDocument(out);p1=doc:openPage(1);p2=doc:openPage(2)
            local reread=p2:getEmbeddedAnnotations()[1];assert(reread.source.num==added.num)
            assert(reread.raw.contents=='source-note');assert(#p1:getEmbeddedAnnotations()==base_count)
            p2:deleteEmbeddedAnnotation(added)
            assert(p2:getEmbeddedAnnotations()==nil,'shrink still has markup')
            local shrunk=dir..'/'..kind..'-shrunk.pdf';doc:writeDocument(shrunk)
            local shr=objects(shrunk)
            if kind=='structure' or kind=='nested' or kind=='combined' then
                assert(not shr[10]:find('/Obj '..added.num..' 0 R',1,true),'dangling OBJR')
                assert(not shr[kind=='nested' and 13 or 11]:find('12 10 0 R',1,true),'dangling ParentTree')
                has(shr[10],'/Obj 6 0 R');has(shr[kind=='nested' and 12 or 11],'0 10 0 R')
            end
            if kind=='popup' or kind=='combined' or kind=='tagged_popup' then assert(not shr[4]:find(ref(dst,'Popup')..' 0 R',1,true),'dangling popup') end
            assert(#p1:getEmbeddedAnnotations()==base_count)
            p2:close();p1:close();doc:close()
            doc=M.openDocument(shrunk);p1=doc:openPage(1);p2=doc:openPage(2)
            assert(p2:getEmbeddedAnnotations()==nil,'shrink did not persist')
            assert(#p1:getEmbeddedAnnotations()==base_count,'shrink changed original page')
            p1:deleteEmbeddedAnnotation(part)
            assert(#p1:getEmbeddedAnnotations()==base_count-1,'delete original must preserve neighbor')
            local deleted=dir..'/'..kind..'-deleted.pdf';doc:writeDocument(deleted)
            local gone=objects(deleted)
            if kind=='structure' or kind=='nested' or kind=='combined' or kind=='tagged_popup' then
                assert(not gone[10]:find('/Obj ',1,true),'delete left original OBJR')
                assert(not gone[kind=='nested' and 12 or 11]:find('0 10 0 R',1,true),'delete left original ParentTree mapping')
            end
            p2:close();p1:close();doc:close()
            doc=M.openDocument(deleted);p1=doc:openPage(1);p2=doc:openPage(2)
            assert(#p1:getEmbeddedAnnotations()==base_count-1,'delete did not persist')
            assert(p2:getEmbeddedAnnotations()==nil,'reopened deleted PDF contains new part')
            assert(added.num~=part.num)
        end
        p2:close();p1:close();doc:close();assert(bytes(file)==input,'source bytes changed')
        for name in require('lfs').dir(dir) do if name~='.' and name~='..' then assert(os.remove(dir..'/'..name)) end end
        assert(require('lfs').rmdir(dir))
        end)
    end

end)

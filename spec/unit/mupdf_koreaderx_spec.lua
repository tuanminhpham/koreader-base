-- Real MuPDF integration: source metadata, snapshot, selective rendering and mutation.
describe("KOReaderX PDF source annotations", function()
    it("preserves originals and resolves/edits only the selected source object", function()
        local ffi = require('ffi')
        require('ffi/loadlib')
        local M = require('ffi/mupdf')
        local DC = require('ffi/drawcontext')
        local work = os.tmpname()
        os.remove(work)
        assert(require('lfs').mkdir(work))
        local function eq(a,b,label) assert(a == b,(label or 'value')..': '..tostring(a)..' ~= '..tostring(b)) end
        local function copy(t) if type(t) ~= 'table' then return t end local n={} for k,v in pairs(t) do n[k]=copy(v) end return n end
        local function bytes(path) local f=assert(io.open(path,'rb')); local d=f:read('*a'); assert(f:close()); return d end
        local function write(path,data) local f=assert(io.open(path,'wb'));assert(f:write(data));assert(f:close()) end
        local function pixels(page)
         local dc=DC.new(); dc:setZoom(1); local w,h=page:getSize(dc)
         local bb=page:draw_new(dc,math.ceil(w),math.ceil(h),0,0)
         local result=ffi.string(bb.data,bb.stride*bb.h);bb:free();return result
        end
        local function pdf_fixture(path, empty_quads, relationship)
         local objects={
          '<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [8 0 R] >> >>',
          '<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>',
          '<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 500] /Contents 5 0 R /Resources << >> /Annots [6 0 R 7 0 R 8 0 R] >>',
          '<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 500] /Contents 5 0 R /Resources << >> >>',
          '<< /Length 0 >>\nstream\n\nendstream',
          '<< /Type /Annot /Subtype /Highlight /Rect [30 450 140 470] /QuadPoints [30 470 140 470 30 450 140 450] /C [1 1 0] /CA 0.5 /F 4 /T (source-author) /Contents (source-note) /CreationDate (D:20260101000000Z) /M (D:20260102000000Z) /XCustom (keep-foreign-value) /XDict << /Nested (keep-dict-value) >> /XRef 10 0 R >>',
          '<< /Type /Annot /Subtype /Underline /Rect [30 350 140 370] /QuadPoints [30 370 140 370 30 350 140 350] /C [0 1 0] /CA 1 /F 4 /T (neighbor-author) /Contents (neighbor-note) >>',
          '<< /Type /Annot /Subtype /Widget /FT /Btn /T (fixture-button) /Rect [250 400 350 430] /P 3 0 R /F 4 /AP << /N 9 0 R >> >>',
          '<< /Type /XObject /Subtype /Form /BBox [0 0 100 30] /Resources << >> /Length 25 >>\nstream\n0 0 1 rg 0 0 100 30 re f\nendstream',
          '<< /Tag (foreign-reference-value) >>',
         }
         if relationship then objects[6] = objects[6]:sub(1,-3)..relationship..' >>' end
         if empty_quads then objects[6] = objects[6]:gsub('/QuadPoints %b[]', '/QuadPoints []') end
         local output='%PDF-1.7\n';local offsets={0}
         for i,obj in ipairs(objects) do offsets[i]=#output;output=output..i..' 0 obj\n'..obj..'\nendobj\n' end
         local xref=#output; output=output..'xref\n0 '..(#objects+1)..'\n0000000000 65535 f \n'
         for _,offset in ipairs(offsets) do output=output..string.format('%010d 00000 n \n',offset) end
         output=output..'trailer\n<< /Size '..(#objects+1)..' /Root 1 0 R >>\nstartxref\n'..xref..'\n%%EOF\n'
         write(path,output)
        end
        local fixture=work..'/source-fixture.pdf';pdf_fixture(fixture)
        local initial=bytes(fixture)
        local doc=M.openDocument(fixture); doc:setColorRendering(true)
        local page=doc:openPage(1)
        local entries,rejected=page:getEmbeddedAnnotations();eq(#entries,2);eq(#rejected,0)
        local part={page=1,num=entries[1].source.num,gen=entries[1].source.gen,raw=copy(entries[1].raw)}
        eq(part.raw.author,'source-author');eq(part.raw.contents,'source-note');eq(part.raw.color_n,3)
        local tilted=copy(part.raw);tilted.quads={{30,40,120,50,28,60,118,70}}
        local tilted_part=page:createEmbeddedAnnotation(tilted)
        local tilted_entry=page:getEmbeddedAnnotations()[3]
        eq(tilted_entry.boxes[1].x,28);eq(tilted_entry.boxes[1].y,40)
        eq(tilted_entry.boxes[1].w,93);eq(tilted_entry.boxes[1].h,31)
        page:deleteEmbeddedAnnotation(tilted_part)
        local baseline=pixels(page)
        doc.annotation_render_filter={[1]={part}}
        local filtered=pixels(page);assert(filtered~=baseline,'filter must change target appearance')
        doc.annotation_render_filter=nil;eq(pixels(page),baseline,'no-filter pixel path')
        -- Pending changes must appear in snapshot but source bytes remain untouched.
        local created=copy(part.raw);created.author='Tác giả tiếng Việt';created.contents='Ghi chú UTF-8';created.color_n=4;created.color_components={0.5,0,0.5,0};created.quads={{200,30,300,30,200,50,300,50}}
        local added=page:createEmbeddedAnnotation(created)
        eq(added.raw.color_n,4)
        local snapshot=work..'/snapshot.pdf';doc:saveSnapshot(snapshot)
        eq(bytes(fixture),initial,'snapshot source bytes')
        local candidate=M.openDocument(snapshot);candidate:setColorRendering(true)
        local cp=candidate:openPage(1);eq(#cp:getEmbeddedAnnotations(),3,'snapshot pending annotations')
        local changed=cp:updateEmbeddedAnnotation(part,{color_n=1,color_components={0.3},author='Đổi tác giả',contents='Đổi ghi chú',type=10})
        eq(changed.num,part.num,'in-place object identity');eq(changed.raw.type,10);eq(changed.raw.color_n,1)
        eq(changed.raw.opacity,part.raw.opacity);eq(changed.raw.creation_date,part.raw.creation_date)
        local ok=pcall(cp.updateEmbeddedAnnotation,cp,part,{contents='stale write'})
        eq(ok,false,'stale identity rejected')
        local no_color=cp:updateEmbeddedAnnotation(changed,{color_n=0,color_components={}})
        eq(no_color.raw.color_n,0)
        local completed=work..'/candidate.pdf';cp:close();candidate:writeDocument(completed);candidate:close()
        assert(bytes(completed):find('keep%-foreign%-value'),'in-place edits preserve foreign dictionary data')
        local reopened=M.openDocument(completed);local rp=reopened:openPage(1);local list=rp:getEmbeddedAnnotations()
        eq(#list,3);eq(list[1].raw.type,10);eq(list[1].raw.color_n,0);eq(list[1].raw.author,'Đổi tác giả')
        eq(list[2].raw.author,'neighbor-author');eq(list[3].raw.author,'Tác giả tiếng Việt')
        rp:close();reopened:close()
        -- Filter pixels equal a candidate where exactly the same source is deleted.
        local deletion=M.openDocument(fixture);deletion:setColorRendering(true);local dp=deletion:openPage(1)
        dp:deleteEmbeddedAnnotation(part);eq(pixels(dp),filtered,'selective rendering vs actual source deletion')
        eq(#dp:getEmbeddedAnnotations(),1,'unrelated annotation preserved');dp:close();deletion:close()
        -- Same geometry, different object identity is never an automatic alias.
        local duplicated=page:createEmbeddedAnnotation(part.raw)
        assert(duplicated.num~=part.num)
        local ambiguous=copy(part);ambiguous.num=0;ambiguous.gen=0
        local resolved,reason=page:resolveEmbeddedAnnotation(ambiguous);eq(resolved,nil);eq(reason,'ambiguous source annotations')
        local after_changed=copy(part);after_changed.raw.contents='externally changed'
        doc.annotation_render_filter={[1]={after_changed}}
        assert(#pixels(page)>0);assert(doc.annotation_render_errors[1][1])
        eq(bytes(fixture),initial,'all candidate and render tests keep source bytes')
        local target_page = doc:openPage(2)
        local template_raw = copy(part.raw)
        template_raw.quads = {{40, 40, 140, 40, 40, 60, 140, 60}}
        template_raw.contents = 'extended note'
        template_raw.type = 11
        local extended = target_page:createEmbeddedAnnotation(template_raw, part)
        eq(extended.page, 2)
        eq(extended.raw.type, 11)
        eq(extended.raw.contents, 'extended note')
        eq(extended.raw.author, part.raw.author)
        target_page:close()
        doc:writeDocument(work..'/template.pdf')
        local template_bytes = bytes(work..'/template.pdf')
        local _, copies = template_bytes:gsub('keep%-foreign%-value', '')
        eq(copies, 2, 'foreign scalar retained on original and new page')
        local _, dictionaries = template_bytes:gsub('keep%-dict%-value', '')
        eq(dictionaries, 2, 'foreign dictionary retained on new page')
        local _, references = template_bytes:gsub('/XRef', '')
        eq(references, 2, 'foreign reference retained on new page')
        local ok_empty = pcall(page.updateEmbeddedAnnotation, page, part, {quads={}})
        eq(ok_empty, false, 'empty edit rejected before changing source')
        page:close();doc:close()
        local invalid_fixture = work..'/invalid.pdf'
        pdf_fixture(invalid_fixture, true)
        local invalid_doc = M.openDocument(invalid_fixture)
        local invalid_page = invalid_doc:openPage(1)
        local valid_entries, invalid_entries = invalid_page:getEmbeddedAnnotations()
        eq(#valid_entries, 1, 'valid neighbor is still enumerated')
        eq(#invalid_entries, 1, 'zero-quad source is explicitly reported')
        invalid_page:close(); invalid_doc:close()
        pdf_fixture(work..'/relationship.pdf', false, ' /IRT 7 0 R /RT /R')
        local relationship_doc = M.openDocument(work..'/relationship.pdf')
        local relationship_page = relationship_doc:openPage(1)
        local relationship_entry = relationship_page:getEmbeddedAnnotations()[1]
        local relationship_part = {page=1, num=relationship_entry.source.num,
            gen=relationship_entry.source.gen, raw=relationship_entry.raw}
        local relationship_target = relationship_doc:openPage(2)
        local accepted, relationship_error = pcall(relationship_target.createEmbeddedAnnotation,
            relationship_target, relationship_entry.raw, relationship_part)
        eq(accepted, true, 'cross-page replies preserve the document-local relationship')
        eq(relationship_error.page, 2)
        eq(#relationship_target:getEmbeddedAnnotations(), 1, 'reply template creates one added part')
        relationship_target:close(); relationship_page:close(); relationship_doc:close()

        print('PASS native metadata RGB/gray/CMYK/no-color, UTF-8 author/note, identity/stale/ambiguous, snapshot pending edits, exact selective render including widget, tilted quad bounds, type/foreign-field/opacity/date roundtrip')

        for _,name in ipairs({'source-fixture.pdf','snapshot.pdf','candidate.pdf','invalid.pdf','template.pdf','relationship.pdf'}) do os.remove(work..'/'..name) end
        assert(require('lfs').rmdir(work))
    end)
end)

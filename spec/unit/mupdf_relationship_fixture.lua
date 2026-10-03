-- Synthetic PDF fixtures; never copies/edits a user document.
local function fixture(path, kind)
    local fields = {
        popup=' /Popup 8 0 R', chain=' /IRT 7 0 R /RT /R', invalid_reply=' /IRT 7 0 R /RT /R', reply=' /IRT 7 0 R /RT /R', group=' /IRT 7 0 R /RT /Group',
        typeless_reply=' /IRT 7 0 R /RT /R', parent=' /Parent 7 0 R', structure=' /StructParent 0', nested=' /StructParent 0',
        combined=' /Popup 8 0 R /IRT 7 0 R /RT /R /Parent 7 0 R /StructParent 0',
        invalid_popup=' /Popup 8 0 R', invalid_structure=' /StructParent 99', cyclic=' /StructParent 0', tagged_popup=' /Popup 8 0 R',
    }
    local objects={
        '<< /Type /Catalog /Pages 2 0 R /StructTreeRoot 9 0 R /MarkInfo << /Marked true >> >>',
        '<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>',
        '<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 500] /Contents 5 0 R /Resources << >> /Annots [6 0 R 7 0 R 8 0 R] >>',
        '<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 500] /Rotate 90 /Contents 5 0 R /Resources << >> >>',
        '<< /Length 0 >>\nstream\n\nendstream',
        '<< /Type /Annot /Subtype /Highlight /P 3 0 R /Rect [30 450 140 470] /QuadPoints [30 470 140 470 30 450 140 450] /C [1 1 0] /F 4 /Contents (source-note) /T (source-author) /XCustom (keep-source)'..assert(fields[kind])..' >>',
        '<< /Type /Annot /Subtype /Underline /P 3 0 R /Rect [30 350 140 370] /QuadPoints [30 370 140 370 30 350 140 350] /C [0 1 0] /F 4 /Contents (thread-root) >>',
        '<< /Type /Annot /Subtype /Popup /P 3 0 R /Parent 6 0 R /Rect [150 350 300 450] /Open true /XPopup (keep-popup) >>',
        '<< /Type /StructTreeRoot /K [10 0 R] /ParentTree 11 0 R /ParentTreeNextKey 12 >>',
        '<< /Type /StructElem /S /Annot /P 9 0 R /Pg 3 0 R /K << /Type /OBJR /Obj 6 0 R /Pg 3 0 R >> >>',
        '<< /Nums [0 10 0 R 7 [10 0 R]] /CustomTree (keep-tree) >>',
        '<< /Nums [0 10 0 R] /Limits [0 0] >>',
        '<< /Nums [7 [10 0 R]] /Limits [7 7] >>',
    }
    if kind == 'nested' then objects[11] = '<< /Kids [12 0 R 13 0 R] /CustomTree (keep-tree) >>' end
    if kind == 'cyclic' then objects[11] = '<< /Kids [11 0 R] /Limits [0 0] >>' end
    if kind == 'invalid_popup' then objects[8] = objects[8]:gsub('/Parent 6 0 R', '/Parent 7 0 R') end
    if kind == 'tagged_popup' then
        objects[8] = objects[8]:gsub(' >>$', ' /StructParent 0 >>')
        objects[10] = objects[10]:gsub('/Obj 6 0 R', '/Obj 8 0 R')
    end
    if kind=='typeless_reply' then objects[7] = objects[7]:gsub('/Type /Annot ', '') end
    if kind=='chain' then
        objects[7] = objects[7]:gsub(' >>$', ' /IRT 14 0 R /RT /R >>')
        objects[14] = objects[7]:gsub(' /IRT 14 0 R /RT /R', '')
        objects[3] = objects[3]:gsub('/Annots %b[]','/Annots [6 0 R 7 0 R 8 0 R 14 0 R]')
    elseif kind=='invalid_reply' then
        objects[7] = objects[7]:gsub(' >>$', ' /IRT 6 0 R /RT /R >>')
    end
    local result='%PDF-1.7\n'; local offsets={}
    for i,obj in ipairs(objects) do offsets[i]=#result; result=result..i..' 0 obj\n'..obj..'\nendobj\n' end
    local xref=#result; result=result..'xref\n0 '..(#objects+1)..'\n0000000000 65535 f \n'
    for _,offset in ipairs(offsets) do result=result..string.format('%010d 00000 n \n',offset) end
    result=result..'trailer\n<< /Size '..(#objects+1)..' /Root 1 0 R >>\nstartxref\n'..xref..'\n%%EOF\n'
    local f=assert(io.open(path,'wb'));assert(f:write(result));assert(f:close())
    return result
end
return fixture

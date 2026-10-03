describe("Owned ordinary PDF annotation provenance", function()
    it("returns serializable creation/update identity and never selects a coincident foreign object", function()
        local ffi = require("ffi")
        require("ffi/loadlib")
        local M = require("ffi/mupdf")
        local path = os.tmpname()
        local fixture = require("mupdf_relationship_fixture")
        fixture(path, "combined")
        local doc = M.openDocument(path)
        local page = doc:openPage(1)
        local foreign = page:getEmbeddedAnnotations()[1]
        local q = foreign.raw.quads[1]
        local points = ffi.new("fz_quad[1]")
        points[0].ul.x, points[0].ul.y, points[0].ur.x, points[0].ur.y = q[1], q[2], q[3], q[4]
        points[0].ll.x, points[0].ll.y, points[0].lr.x, points[0].lr.y = q[5], q[6], q[7], q[8]
        local created = page:addMarkupAnnotation(points, 1, 8)
        assert.is_table(created.raw)
        assert.is_number(created.source.num)
        assert.is_not.equals(foreign.source.num, created.source.num)
        assert.is_nil(created.raw.author)
        local part = { page = 1, num = created.source.num, gen = created.source.gen, raw = created.raw }
        local absent = {page=1,num=99999,gen=0,raw=created.raw}
        assert.truthy(page:resolveEmbeddedAnnotation(absent)) -- generic imported relocation
        assert.is_nil(page:resolveEmbeddedAnnotation(absent, true)) -- never claim coincident ownership
        local pointer = assert(page:resolveEmbeddedAnnotation(part, true))
        local updated = page:updateMarkupAnnotation(pointer, "Owned note")
        assert.equals(created.source.num, updated.source.num)
        assert.equals("Owned note", updated.raw.contents)
        assert.is_nil(page:resolveEmbeddedAnnotation(part))
        local retained = page:getEmbeddedAnnotations()[1]
        assert.are.same(foreign, retained)
        part.raw = updated.raw
        page:close(); doc:writeDocument(path); doc:close()
        doc = M.openDocument(path); page = doc:openPage(1)
        assert.truthy(page:resolveEmbeddedAnnotation(part))
        page:deleteEmbeddedAnnotation(part)
        assert.equals(2, #page:getEmbeddedAnnotations())
        assert.are.same(foreign, page:getEmbeddedAnnotations()[1])
        page:close(); doc:close(); os.remove(path)
    end)
end)

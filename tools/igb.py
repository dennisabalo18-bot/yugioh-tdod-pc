"""Lector de archivos IGB de Alchemy (los modelos del juego), con los campos
de cada objeto decodificados por su meta-objeto.

Formato (version 0x80000005, little endian):
- cabecera: 12 u32 (entradas, meta-objetos, objetos, memoria, meta-campos...,
  0xFADA, version);
- meta-campos: 12 bytes c/u (largo del nombre, version) + los nombres;
- bloque de alineaciones (su tamano primero);
- meta-objetos: 24 bytes c/u (largo del nombre, version, campos, padre,
  ranuras) + nombre + 6 bytes por campo (meta-campo, ranura, tamano);
- entradas del directorio y objetos: (meta-objeto, tamano, campos); un u32
  entre las dos listas;
- la memoria al final: el bloque de cada igMemoryDirEntry, en orden.

Campos de un objeto: en el orden del meta-objeto (heredados primero), cada
uno alineado a min(tamano, 4); un igString es u32 largo + bytes. Un
ObjectRef / MemoryRef es el numero de la entrada del directorio (-1 = nada).

    obj = IGB(path); obj.objs[i] -> (clase, {campo: valor}), obj.mem_bytes(ref)
"""
import struct

SIZES = {"igShortMetaField": 4,  # el short ocupa 4 en el archivo
         "igBoolMetaField": 1, "igUnsignedCharMetaField": 1,
         "igCharMetaField": 1, "igUnsignedShortMetaField": 2}


class IGB:
    def __init__(self, path_or_bytes):
        d = path_or_bytes if isinstance(path_or_bytes, (bytes, bytearray)) else open(path_or_bytes, "rb").read()
        self.d = d
        eb, ec, mob, moc, ob, oc, mb, mc, mfb, mfc, magic, ver = struct.unpack_from("<12I", d, 0)
        if magic != 0xFADA:
            raise ValueError("no es IGB")
        p = 0x30
        mf = [struct.unpack_from("<3I", d, p + 12 * i) for i in range(mfc)]
        p += 12 * mfc
        self.mfn = []
        for nl, _a, _b in mf:
            self.mfn.append(d[p:p + nl].decode("latin-1").strip("\0"))
            p += nl
        p += struct.unpack_from("<I", d, p)[0]
        hdr = [struct.unpack_from("<6i", d, p + 24 * i) for i in range(moc)]
        q = p + 24 * moc
        self.meta = []          # (nombre, padre, [(tipo, ranura, tamano)])
        for nl, _ma, _mi, fc, par, _sc in hdr:
            name = d[q:q + nl].decode("latin-1").strip("\0 ")
            q += nl
            flds = [struct.unpack_from("<3H", d, q + 6 * j) for j in range(fc)]
            q += 6 * fc
            self.meta.append((name, par, [(self.mfn[t], s, z) for t, s, z in flds]))
        ents = []
        self.ent_off = []       # donde empiezan los campos de cada entrada
        for _ in range(ec):
            t, sz = struct.unpack_from("<II", d, q)
            ents.append((self.meta[t][0], d[q + 8:q + sz]))
            self.ent_off.append(q + 8)
            q += sz
        q += 4
        raw = []
        self.obj_off = []       # donde empiezan los campos de cada objeto
        for _ in range(oc):
            t, sz = struct.unpack_from("<II", d, q)
            raw.append((t, d[q + 8:q + sz]))
            self.obj_off.append(q + 8)
            q += sz
        self.mem = {}
        self.ent_obj = {}       # entrada del directorio -> indice de objeto
        base = q
        self.mem_base = base
        self.mem_order = []     # entradas de memoria, en el orden de los bloques
        for i, (n, b) in enumerate(ents):
            if n == "igMemoryDirEntry":
                sz = struct.unpack_from("<i", b, 4)[0]
                q += (-(q - base)) % 4      # cada bloque empieza alineado a 4
                self.mem[i] = (q, sz)
                self.mem_order.append(i)
                q += sz
            elif n == "igObjectDirEntry":
                self.ent_obj[i] = len(self.ent_obj)
        self.objs = [(self.meta[t][0], self._fields(t, b)) for t, b in raw]

    def _fields(self, t, b):
        out = {}
        offs = {}
        p = 0
        for typ, slot, size in self.meta[t][2]:
            size = SIZES.get(typ, size)
            al = min(size, 4)
            p = (p + al - 1) // al * al
            offs[slot] = p
            if typ == "igStringMetaField":
                n = struct.unpack_from("<I", b, p)[0]
                out[slot] = b[p + 4:p + 4 + n].split(b"\0")[0].decode("latin-1")
                p += 4 + n
                continue
            if typ in ("igMatrix44fMetaField",):
                v = struct.unpack_from("<16f", b, p)
            elif typ in ("igVec3fMetaField",):
                v = struct.unpack_from("<3f", b, p)
            elif typ == "igVec4fMetaField":
                v = struct.unpack_from("<4f", b, p)
            elif typ in ("igFloatMetaField",):
                v = struct.unpack_from("<f", b, p)[0]
            elif typ == "igDoubleMetaField":
                v = struct.unpack_from("<d", b, p)[0]
            elif typ == "igLongMetaField" and size == 8:
                v = struct.unpack_from("<q", b, p)[0]
            elif size == 4:
                v = struct.unpack_from("<i", b, p)[0]
            elif size == 2:
                v = struct.unpack_from("<h", b, p)[0]
            elif size == 1:
                v = b[p]
            else:
                v = b[p:p + size]
            out[slot] = v
            p += size
        out["_len"] = (p, len(b))
        out["_off"] = offs
        return out

    def obj(self, ref):
        """el objeto (clase, campos) de una entrada del directorio, o None"""
        i = self.ent_obj.get(ref)
        return None if i is None else self.objs[i]

    def mem_bytes(self, ref):
        if ref not in self.mem:
            return b""
        o, s = self.mem[ref]
        return self.d[o:o + s]

    def list_refs(self, ref):
        """los ObjectRef de un igObjectList (campos 3 cantidad, 5 memoria)"""
        o = self.obj(ref)
        if not o:
            return []
        n = o[1][3]
        return list(struct.unpack_from("<%di" % n, self.mem_bytes(o[1][5]), 0)) if n else []


class IGBEdit:
    """Cambia un IGB: campos de objetos (mismo tamano, en su lugar) y bloques de
    memoria (cualquier tamano: la memoria se rearma al guardar, alineada a 4,
    y se corrige el tamano en su igMemoryDirEntry)."""

    def __init__(self, path):
        self.g = IGB(path)
        self.d = bytearray(self.g.d)
        self.new_mem = {}

    def field_off(self, ref, slot):
        i = self.g.ent_obj[ref]
        return self.g.obj_off[i] + self.g.objs[i][1]["_off"][slot]

    def set_i32(self, ref, slot, v):
        struct.pack_into("<i", self.d, self.field_off(ref, slot), v)

    def set_mat(self, ref, slot, m16):
        struct.pack_into("<16f", self.d, self.field_off(ref, slot), *[float(x) for x in m16])

    def set_mem(self, ref, data):
        if ref not in self.g.mem:
            raise KeyError("no es un bloque de memoria: %d" % ref)
        self.new_mem[ref] = bytes(data)

    def mem(self, ref):
        if ref in self.new_mem:
            return self.new_mem[ref]
        o, s = self.g.mem[ref]
        return bytes(self.d[o:o + s])

    def save(self, path):
        g = self.g
        head = bytearray(self.d[:g.mem_base])
        body = bytearray()
        for i in g.mem_order:
            while len(body) % 4:
                body.append(0)
            blk = self.mem(i)
            body += blk
            struct.pack_into("<i", head, g.ent_off[i] + 4, len(blk))
        tail_extra = len(self.d) - max(o + s for o, s in g.mem.values())
        out = bytes(head + body) + bytes(self.d[len(self.d) - tail_extra:]) if tail_extra > 0 else bytes(head + body)
        # cabecera: el campo 6 es el largo de la memoria (con el relleno)
        hdr = list(struct.unpack_from("<12I", out, 0))
        hdr[6] = len(body)
        out = struct.pack("<12I", *hdr) + out[48:]
        open(path, "wb").write(out)
        return out

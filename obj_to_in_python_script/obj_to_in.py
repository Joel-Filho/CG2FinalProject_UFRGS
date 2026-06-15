#obj reading based off https://www.pygame.org/wiki/OBJFileLoader

import math
    
def load_obj_file(file_path):
    vertices = []
    normals = []
    texcoords = []
    tris = []

    with open(file_path, 'r') as file:
        for line in file:
            if line.startswith('#'): continue
            values = line.split()
            if not values: continue
            if values[0] == 'v':
                vertices.append([float(x) for x in values[1:4]])
            elif values[0] == 'vn':
                normals.append([float(x) for x in values[1:4]])
            elif values[0] == 'vt':
                texcoords.append([float(x) for x in values[1:3]])
            elif values[0] == 'f':
                face = []
                texcoord = []
                norms = []
                for v in values[1:]:
                    w = v.split('/')
                    face.append(int(w[0]))
                    if len(w) >= 2 and len(w[1]) > 0:
                        texcoord.append(int(w[1]))
                    else:
                        texcoord.append(0)
                    if len(w) >= 3 and len(w[2]) > 0:
                        norms.append(int(w[2]))
                    else:
                        norms.append(0)
                tris.append((face, norms, texcoord))

    return vertices, normals, texcoords, tris
    
def save_in_file(filename, vertices, normals, texcoords, tris): 
    with open(filename, 'w') as f:
        f.write("Object name = CONVERTED OBJ\n")#placeholder since it's unused
        
        f.write(f"# triangles = {len(tris)}\n")
        
        f.write("Material count = 1\n")#placeholder since it's unused
        f.write("ambient color 0.2 0.0 0.0\n")
        f.write("diffuse color 1.0 0.0 0.0\n")
        f.write("specular color 1.0 1.0 1.0\n")
        f.write("material shine 8.0\n")
        
        has_texture = len(texcoords) > 0
        f.write(f"Texture = {"YES" if has_texture else "NO"}\n")
        
        f.write("-- 3*[pos(x,y,z) normal(x,y,z) color_index text_coord] face_normal(x,y,z)\n")
        
        for tri in tris:
            vert = tri[0]
            norm = tri[1]
            texc = tri[2]
            for i in range(3):
                f.write(f"v{i} ")
                # vertex positions
                for j in vertices[vert[i] - 1]:
                    f.write(f"{j:f} ")
                # vertex normals
                for j in normals[norm[i] - 1]:
                    f.write(f"{j:f} ")
                # color index (0)
                f.write("0 ")#placeholder since it's unused
                # texture coords
                if has_texture:
                    for j in texcoords[texc[i] - 1]:
                        if j > 1.0:
                            j = j - math.floor(j)
                        f.write(f"{j:f} ")
                f.write("\n")
            # face normal
            f.write("face normal 0.0 0.0 1.0\n")#placeholder since it's unused

v, n, tc, tr = load_obj_file("example_objs/oguri.obj")

"""
#debug prints
print("vertices:")
for vert in v:
    print(vert)
    
print("normals:")
for norm in n:
    print(norm)
    
print("texcoords:")
for texc in tc:
    print(texc)
    
print("triangle indexes:")
for tri in tr:
    print(tri)
"""
    
save_in_file("oguri.in", v, n, tc, tr)
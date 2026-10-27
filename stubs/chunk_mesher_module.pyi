from typing import Optional

from block_module import BlockManager
from chunk_module import Chunk
from mesh_module import Mesh
from texture_module import TextureBlocksManager


class ChunkMesher:
    @staticmethod
    def build_mesh(
        chunk: Chunk, block_mgr: BlockManager, tex_mgr: TextureBlocksManager
    ) -> Optional[Mesh]: ...

    def __repr__(self) -> str: ...

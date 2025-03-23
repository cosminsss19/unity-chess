using UnityEngine;

namespace DefaultNamespace
{
    public static class Constants
    {
        public static Vector3 offset = new(0, 0.5f, 0);
        public static Vector2Int forward = new(0, 1);
        public static Vector2Int back = new(0, -1);
        public static Vector2Int right = new(1, 0);
        public static Vector2Int left = new(-1, 0);
        public static Vector2Int forwardRight = new(1, 1);
        public static Vector2Int forwardLeft = new(-1, 1);
        public static Vector2Int backRight = new(1, -1);
        public static Vector2Int backLeft = new(-1, -1);
    }
}
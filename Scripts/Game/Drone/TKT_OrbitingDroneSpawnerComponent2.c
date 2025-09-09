class TKT_OrbitingDroneSpawnerComponent2Class : ScriptComponentClass {}

class TKT_OrbitingDroneSpawnerComponent2 : ScriptComponent 
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Drone prefab to spawn", "et")]
	protected ResourceName m_dronePrefab;

	[Attribute(defvalue: "30.0", desc: "Orbit height above anchor (meters)")]
	protected float m_height;

	[Attribute(defvalue: "25.0", desc: "Orbit radius (meters)")]
	protected float m_radius;

	[Attribute(defvalue: "12.0", desc: "Orbit speed (m/s)")]
	protected float m_speed;

	[Attribute(defvalue: "1", desc: "Counter-Clockwise (1) or Clockwise (0)")]
	protected bool m_counterClockwise;

	[Attribute(defvalue: "1.0", desc: "Banking multiplier (0=flat, 1=realistic-ish)")]
	protected float m_bankScale;

	[Attribute(defvalue: "0.5", desc: "Respawn delay if drone is destroyed (s)")]
	protected float m_respawnDelay;

	[Attribute(defvalue: "0.0", desc: "Orbit yaw offset (degrees)")]
	protected float m_yawOffsetDeg;
	
	[Attribute(defvalue: "1", desc: "Draw debug for current + queued segments")]
	protected bool m_debugDraw;

	// --- state ---
	protected ref array<ref TKT_PathSegment> m_path = {};
	protected float m_segTime;
	protected ref TKT_PathSegment m_curr;
	protected IEntity m_drone;
	protected float   m_theta;
	protected vector  m_prevPos;
	protected bool    m_wasSpawned;

	protected const float G = 9.81;
	protected const float DEG2RAD = 0.017453292519943295; // pi/180
	
	protected void BuildDemoPath()
	{
		vector A = GetOwner().GetOrigin();
		vector B = A + Vector(60, 0, 40);
	
		// Orbit A for 3 loops
		ref TKT_OrbitSegment orbA = new TKT_OrbitSegment();
		orbA.m_center = A; orbA.m_height = m_height; orbA.m_radius = m_radius;
		orbA.m_speed = m_speed; orbA.m_ccw = m_counterClockwise; orbA.m_yawDeg = m_yawOffsetDeg;
		orbA.m_loops = 1.0;
		m_path.Insert(orbA);
	
		// *** Make the line fly at orbit height ***
		vector A3D = A + Vector(0, m_height, 0);
		vector B3D = B + Vector(0, m_height, 0);
		vector A_edge = A3D + Vector(m_radius, 0, 0);
		vector B_edge = B3D + Vector(m_radius, 0, 0);
	
		ref TKT_LineSegment leg = new TKT_LineSegment();
		leg.m_a = A_edge;
		leg.m_b = B_edge;
		leg.m_speed = m_speed;
		leg.Init();
		m_path.Insert(leg);
	
		// Orbit B forever
		ref TKT_OrbitSegment orbB = new TKT_OrbitSegment();
		orbB.m_center = B; orbB.m_height = m_height; orbB.m_radius = m_radius;
		orbB.m_speed = m_speed; orbB.m_ccw = m_counterClockwise; orbB.m_yawDeg = m_yawOffsetDeg;
		orbB.m_loops = 0;
		m_path.Insert(orbB);
	}
	
	protected void EnsureCurrentSegment()
	{
		if (!m_curr && m_path.Count() > 0)
		{
			m_curr = m_path[0];
			m_segTime = 0;
			m_path.RemoveOrdered(0);
		}
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.FRAME);
		
		if (m_path.IsEmpty())
			BuildDemoPath();
		
		if (IsMaster())
			SpawnDrone();
	}

	override void OnDelete(IEntity owner)
	{
		super.OnDelete(owner);
		if (IsMaster())
			SafeDelete(m_drone);
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!IsMaster()) return;

		if (!m_drone && m_wasSpawned)
		{
			GetGame().GetCallqueue().CallLater(SpawnDrone, Math.Round(m_respawnDelay * 1000), false);
			m_wasSpawned = false;
			return;
		}

		if (!m_drone) return;
		
		EnsureCurrentSegment();
		if (!m_curr) return;
		
		vector pos, vel, fwd, up;
		bool keepGoing = m_curr.Eval(m_segTime, pos, vel, fwd, up);
		m_segTime += timeSlice;
		
		// set transform from segment output
		// (re-orthonormalize so culling never glitches)
		vector right = Normalize(Cross(up, fwd));
		up = Normalize(Cross(fwd, right));
		
		vector tr[4];
		tr[0] = right; tr[1] = up; tr[2] = fwd; tr[3] = pos;
		m_drone.SetTransform(tr);
		
		if (m_debugDraw)
		{
			// current segment in bright colors
			m_curr.DebugDraw(m_segTime);
		
			// queued segments in dimmer colors (optional quick pass)
			for (int i = 0; i < m_path.Count(); i++)
			{
				// visual preview at t=0 to show where they are
				m_path[i].DebugDraw(0.0);
			}
		
			// draw axes at the drone to see attitude
			const int flags = ShapeFlags.ONCE | ShapeFlags.NOZBUFFER;
			TKT_DrawLine(pos, pos + tr[0] * 2.0, 0xFFFF0000); // right
			TKT_DrawLine(pos, pos + tr[1] * 2.0, 0xFF00FF00); // up
			TKT_DrawLine(pos, pos + tr[2] * 3.0, 0xFF0000FF); // forward
		}
		
		// if this segment ended, move to next next frame
		if (!keepGoing) {
			// hand off the exact end position to the next segment if it's a line
			if (m_path.Count() > 0) {
				TKT_LineSegment nextLine = TKT_LineSegment.Cast(m_path[0]);
				if (nextLine) { nextLine.m_a = pos; nextLine.Init(); }
			}
			m_curr = null;
		}
		
		m_prevPos = pos;
		
		if ((int)(m_segTime*10)%10==0) PrintFormat("[Drone] t=%2 pos=%1", m_segTime, pos);
	}

	protected void SpawnDrone()
	{
		if (m_drone || !m_dronePrefab) return;

		Resource res = Resource.Load(m_dronePrefab);
		if (!res) { Print("[TKT_Orbit] failed to load drone prefab"); return; }

		EntitySpawnParams p = new EntitySpawnParams();
		p.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(p.Transform);
		p.Transform[3] = GetOwner().GetOrigin() + Vector(0, m_height, 0);

		m_drone = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), p);
		if (!m_drone) { Print("[TKT_Orbit] spawn returned null"); return; }

		RplComponent rpl = RplComponent.Cast(m_drone.FindComponent(RplComponent));
		if (!rpl) Print("[TKT_Orbit] WARNING: drone has no RplComponent; clients won't see it");

		m_wasSpawned = true;
		m_prevPos = m_drone.GetOrigin();
	}

	// ---------- helpers ----------
	protected bool IsMaster()
	{
		RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
		if (rpl) return rpl.IsMaster();
		return Replication.IsServer();
	}

	protected static vector Normalize(vector v)
	{
		float len = v.Length();
		if (len <= 0.0001) return "0 0 0";
		return v / len;
	}

	protected static float Dot(vector a, vector b)
	{
		return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
	}

	protected static vector Cross(vector a, vector b)
	{
		// (ay*bz - az*by, az*bx - ax*bz, ax*by - ay*bx)
		return Vector(
			a[1]*b[2] - a[2]*b[1],
			a[2]*b[0] - a[0]*b[2],
			a[0]*b[1] - a[1]*b[0]
		);
	}

	protected void SafeDelete(IEntity ent)
	{
		if (!ent) return;
		RplComponent rpl = RplComponent.Cast(ent.FindComponent(RplComponent));
		if (rpl) RplComponent.DeleteRplEntity(ent, true);
	}
}

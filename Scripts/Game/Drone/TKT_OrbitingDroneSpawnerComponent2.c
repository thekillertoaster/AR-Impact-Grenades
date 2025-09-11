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
	
	[Attribute(defvalue: "1", desc: "Second orbit CCW (1) or CW (0)")]
	protected bool m_counterClockwiseB;

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
	
	protected void BuildDemoPath()
	{
		// centers
		vector A = GetOwner().GetOrigin();
		vector B = A + Vector(0, 30, 100);
	
		float rA = m_radius;
		float rB = m_radius;     // change if the second orbit is a different size
		float hA = m_height;
		float hB = m_height;     // change if second orbit altitude differs
	
		vector A3 = A + Vector(0, hA, 0);
		vector B3 = B + Vector(0, hB, 0);
	
		// compute both mirror tangents
		vector p1L, p2L, dirL, p1R, p2R, dirR;
		bool okL = TKT_ComputeCircleTangent(A3, rA, m_counterClockwise,
		                                    B3, rB, m_counterClockwiseB,
		                                    /*chooseUpper*/ true,
		                                    p1L, p2L, dirL);
		bool okR = TKT_ComputeCircleTangent(A3, rA, m_counterClockwise,
		                                    B3, rB, m_counterClockwiseB,
		                                    /*chooseUpper*/ false,
		                                    p1R, p2R, dirR);
	
		if (!okL && !okR) { Print("[Path] No valid tangents"); return; }
	
		// score alignment at BOTH circles
		float sAL = -2.0, sAR = -2.0, sBL = -2.0, sBR = -2.0;
		if (okL) sAL = TKT_ScoreTangentAlign(A3, m_counterClockwise,  p1L, dirL);
		if (okR) sAR = TKT_ScoreTangentAlign(A3, m_counterClockwise,  p1R, dirR);
		if (okL) sBL = TKT_ScoreTangentAlign(B3, m_counterClockwiseB, p2L, dirL);
		if (okR) sBR = TKT_ScoreTangentAlign(B3, m_counterClockwiseB, p2R, dirR);
		
		// lengths for tie-break
		float lenL; if (okL) lenL =  (p2L - p1L).Length(); else lenL = 1e9;
		float lenR; if (okR) lenR =  (p2R - p1R).Length(); else lenR = 1e9;
		
		// pick tangent: prefer both ends aligned (>=0), else higher min alignment, else shorter
		float qL = sAL; if (sBL < qL) qL = sBL;
		float qR = sAR; if (sBR < qR) qR = sBR;
		
		bool useL;
		if (okL && !okR) useL = true;
		else if (!okL && okR) useL = false;
		else {
			if (qL >= 0.0 && qR < 0.0) useL = true;
			else if (qR >= 0.0 && qL < 0.0) useL = false;
			else {
				if (qL > qR) useL = true;
				else if (qR > qL) useL = false;
				else { if (lenL <= lenR) useL = true; else useL = false; }
			}
		}
		
		vector Pexit, Penter, legDir;
		if (useL) { Pexit = p1L; Penter = p2L; legDir = dirL; }
		else        { Pexit = p1R; Penter = p2R; legDir = dirR; }
		
		// ORBIT A — start exactly at Pexit so we leave cleanly into the leg
		ref TKT_OrbitSegment orbA = new TKT_OrbitSegment();
		orbA.m_center = A; orbA.m_height = hA; orbA.m_radius = rA;
		orbA.m_speed = m_speed; orbA.m_ccw = m_counterClockwise; orbA.m_yawDeg = 0.0;
		orbA.m_loops = 2.0;
		orbA.SetStartAtPoint(Pexit);
		m_path.Insert(orbA);
	
		// STRAIGHT LEG (swap for a Hermite segment later if you want curvature easing)
		ref TKT_LineSegment leg = new TKT_LineSegment();
		leg.m_a = Pexit; leg.m_b = Penter; leg.m_speed = m_speed; leg.Init();
		m_path.Insert(leg);
	
		// ORBIT B — start at Penter so the handoff is seamless
		ref TKT_OrbitSegment orbB = new TKT_OrbitSegment();
		orbB.m_center = B; orbB.m_height = hB; orbB.m_radius = rB;
		orbB.m_speed = m_speed; orbB.m_ccw = m_counterClockwiseB; orbB.m_yawDeg = 0.0;
		orbB.m_loops = 0;
		orbB.SetStartAtPoint(Penter);
		m_path.Insert(orbB);
	
		// optional debug markers
		if (m_debugDraw) {
			const int flags = ShapeFlags.ONCE | ShapeFlags.NOZBUFFER;
			Shape.CreateSphere(0xFF00FFFF, flags, Pexit,  0.35);
			Shape.CreateSphere(0xFFFF00FF, flags, Penter, 0.35);
			
			TKT_DrawLine(A3, Pexit,  0xFFAA8800);
			TKT_DrawLine(B3, Penter, 0xFFAA8800);
			
			TKT_DrawLine(Pexit, Penter, 0xFF00FF00);
		}
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
		vector right = TKT_Normalize(TKT_Cross(up, fwd));
		up = TKT_Normalize(TKT_Cross(fwd, right));
		
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
			m_curr = null;
		}
		
		m_prevPos = pos;
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

	protected void SafeDelete(IEntity ent)
	{
		if (!ent) return;
		RplComponent rpl = RplComponent.Cast(ent.FindComponent(RplComponent));
		if (rpl) RplComponent.DeleteRplEntity(ent, true);
	}
}

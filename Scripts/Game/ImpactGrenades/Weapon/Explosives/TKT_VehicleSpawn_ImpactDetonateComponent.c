class TKT_VehicleSpawn_ImpactDetonateComponentClass :  TKT_ImpactDetonateComponentClass {}

class TKT_VehicleSpawn_ImpactDetonateComponent : TKT_ImpactDetonateComponent 
{
	
	[Attribute("", UIWidgets.ResourceNamePicker, "Vehicle prefab", "et")]
	protected ResourceName m_vehiclePrefab;
	
	protected void DespawnEntity(IEntity ent)
	{
		if (!ent) return;
		RplComponent rpl = RplComponent.Cast(ent.FindComponent(RplComponent));
		if (rpl) RplComponent.DeleteRplEntity(ent, true);
	}
	
	protected void SpawnVehicleAt(vector pos)
	{
		// only the authority should spawn replicated effects
		if (m_rpl && !m_rpl.IsMaster()) return;
		if (!m_vehiclePrefab) return;
	
		Resource res = Resource.Load(m_vehiclePrefab);
		if (!res) return;
	
		BaseWorld world = GetGame().GetWorld();
		EntitySpawnParams p = new EntitySpawnParams();
		p.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(p.Transform);
		p.Transform[3] = pos;
	
		IEntity vehicleEnt = GetGame().SpawnEntityPrefab(res, world, p);
	}
	
	override void EOnContact(IEntity owner, IEntity other, Contact contact)
	{
		if(!MeetsRequirements(false)) return;

		m_done = true;
		ClearEventMask(owner, EntityEvent.CONTACT | EntityEvent.FRAME);
		vector hitPos = owner.GetOrigin();
		SpawnVehicleAt(hitPos + Vector(0, 2, 0)); 
		
		// delete grenade
		GetGame().GetCallqueue().CallLater(DespawnEntity, 0, false, owner);
	}
}


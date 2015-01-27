<?php
/**
 * @file
 * Action to merge taxonomy terms
 */

function _dennis_term_manager_process_action_merge($data) {

  //print_r($data);

  // Get node data.
  if(isset($data[nid])) {
   $node = node_load($data['nid']);
  }

  // Set node change status.
  $node_changed = FALSE;
  // Get node type info.
  if ($entity_info = field_info_instances('node', $node->type)) {
    foreach ($entity_info as $info) {
      // Check each node field for term reference.
      if($info['display']['default']['type'] == 'taxonomy_term_reference_link') {
        foreach ($node->$info[field_name] as $tids) {
         foreach ($tids as $tid => $value) {
            if($value['tid'] == $data['target_tid']) {
              //print_r($node->{$info['field_name']}['und'][$tid]['tid']);
              $node->{$info['field_name']}['und'][$tid]['tid'] = $data['source_tid'];
              $node_changed = TRUE;
            }
          }
        }
      }
    }
    //print_r($node->field_artile_type);
    // print_r($data['source_tid']);
    if($node_changed) {
      node_save($node);
    }
  }


  // Push each node into Drupal queue

}

/**
 * Get each node which has target taxonomy term reference.
 */
function _dennis_term_manager_get_merging_nodes($data) {
  // Get vocabulary
  $vocabulary = dennis_term_manager_get_vocabulary($data['vocabulary_name']);
  if (!isset($vocabulary['machine_name']) || !isset($vocabulary['vid'])) {
    drupal_set_message(t("There is no vocabulary @vocab exists in database for create action on row @row", array('@row' => $row, '@vocab' => $data['vocabulary_name'])));
  }
  // Get target term
  $target_terms = explode("&gt;-&gt;", $data['target']);
  //check if target term exists.
  if (count($target_terms) <= 1) {
    drupal_set_message(t("Target Term Name is not valid in row @row", array('@row' => $data['row'])));
    return;
  }
  $target_term = $target_terms[1];
  $target_vocabulary_name = $target_terms[0];

 // Get target vocabulary
  $target_vocabulary = dennis_term_manager_get_vocabulary($target_vocabulary_name);
  if (!isset($target_vocabulary['machine_name']) || !isset($target_vocabulary['vid'])) {
    drupal_set_message(t("There is no vocabulary @vocab exists in database for create action on row @row", array('@row' => $row, '@vocab' => $data['vocabulary_name'])));
  }

  // Check target and source term belongs to same vocabulary.
  if ($vocabulary['vid'] != $target_vocabulary['vid']) {
    drupal_set_message(t("Target Term and source term does not belong to same vocabulary in row @row", array('@row' => $data['row'])));
    return;
  }

  // Get target tid by term_name
  $target_terms = taxonomy_get_term_by_name($target_term, $vocabulary['machine_name']);
  reset($target_terms);
  $target_term_id = key($target_terms);

  if (!isset($target_term_id)) {
    drupal_set_message(t("Target term '@term_name' does not exist in vocabulary '@vocab'", array('@term_name' => $target_term, '@vocab' => $data['vocabulary_name'])));
    return;
  }

  // Check validity of source term
  $terms = taxonomy_get_term_by_name($data['term_name'], $vocabulary['machine_name']);
  reset($terms);
  $term_id = key($terms);

  if (!isset($term_id)) {
    drupal_set_message(t("Term '@term_name' does not exist in vocabulary '@vocab'", array('@term_name' => $data['term_name'], '@vocab' => $data['vocabulary_name'])));
    return;
  }
  // Get node from taxonomy_index using target term tid
  $query = db_select('taxonomy_index', 'ti');
  $query->fields('ti', array('tid','nid'));
  $query->condition('ti.tid', $target_term_id, '=');
  $nodes = $query->execute()->fetchAll();
  $terms_info = array (
    'target_tid' => $target_term_id,
    'vid' => $vocabulary['vid'],
    'source_tid' => $term_id,
    );
  return array($nodes, $terms_info);
}